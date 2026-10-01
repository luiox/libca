#include "libca/zip/file.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "libca/zip/checksum.hpp"
#include "libca/zip/detail/inflate_raw.hpp"

namespace ca::zip {

namespace {

constexpr ca::u32   kCenSig           = 0x02014b50;
constexpr ca::u32   kLocSig           = 0x04034b50;
constexpr ca::u32   kEndSig           = 0x06054b50;
constexpr ca::u32   kZip64EndSig      = 0x06064b50;
constexpr ca::u32   kZip64LocatorSig  = 0x07064b50;
constexpr ca::usize kMinEndSize       = 22;
constexpr ca::usize kMaxCommentSize   = 0xFFFF;
constexpr ca::usize kZip64LocatorSize = 20;
constexpr ca::usize kZip64EndMinSize  = 56;

ca::u16 read_u16(const ca::u8* p) {
    return static_cast<ca::u16>(p[0] | (p[1] << 8));
}

ca::u32 read_u32(const ca::u8* p) {
    return static_cast<ca::u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

ca::u64 read_u64(const ca::u8* p) {
    return static_cast<ca::u64>(read_u32(p)) | (static_cast<ca::u64>(read_u32(p + 4)) << 32);
}

// 构造路径保留异常（见 ZipFile 头文件注释）：把 open 的错误转为 std::runtime_error。
void throw_if_err(Result<void, ZipErrorInfo> result) {
    if (result.is_err()) {
        throw std::runtime_error(std::move(result).unwrap_err().message);
    }
}

ca::Result<ca::u32, ZipErrorInfo> checked_cast_u32(ca::u64 value, const char* what) {
    if (value > std::numeric_limits<ca::u32>::max()) {
        return Err(ZipErrorInfo{ZipError::UNSUPPORTED, std::string(what) + " exceeds 32-bit limit"});
    }
    return Ok(static_cast<ca::u32>(value));
}

// 解析 ZIP64 extra field（tag 0x0001）中第 fieldIndex 个出现的字段。
// 哪些字段在场由 has* 标志决定，顺序固定：uncompressed → compressed → offset。
bool read_zip64_extra_field_value(const std::vector<ca::u8>& extra_field, bool has_uncompressed_size,
                                  bool has_compressed_size, bool has_local_header_offset, int field_index,
                                  ca::u64& out_value) {
    ca::usize offset = 0;
    while (offset + 4 <= extra_field.size()) {
        const ca::u16   tag       = read_u16(extra_field.data() + offset);
        const ca::u16   size      = read_u16(extra_field.data() + offset + 2);
        const ca::usize dataStart = offset + 4;
        const ca::usize dataEnd   = dataStart + size;
        if (dataEnd > extra_field.size()) {
            return false;
        }

        if (tag == 0x0001u) {
            ca::usize cursor = dataStart;
            for (int currentField = 0; currentField <= field_index; ++currentField) {
                const bool present = (currentField == 0 && has_uncompressed_size) ||
                                     (currentField == 1 && has_compressed_size) ||
                                     (currentField == 2 && has_local_header_offset);
                if (!present) {
                    if (currentField == field_index) {
                        return false;
                    }
                    continue;
                }
                if (cursor + 8 > dataEnd) {
                    return false;
                }
                if (currentField == field_index) {
                    out_value = read_u64(extra_field.data() + cursor);
                    return true;
                }
                cursor += 8;
            }
            return false;
        }

        offset = dataEnd;
    }
    return false;
}

bool is_valid_end_candidate(const std::vector<ca::u8>& data, ca::usize eocd_pos) {
    if (eocd_pos + kMinEndSize > data.size()) {
        return false;
    }
    const ca::u8* eocd       = &data[eocd_pos];
    ca::u16       commentLen = read_u16(eocd + 20);
    return eocd_pos + kMinEndSize + commentLen == data.size();
}

// JVM ZipFile 语义：从尾部向前找最后一条合法 EOCD（注释长度自洽）。
Result<ca::usize, ZipErrorInfo> find_jvm_style_end(const std::vector<ca::u8>& data) {
    if (data.size() < kMinEndSize) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP file too small"});
    }

    ca::usize searchStart =
        data.size() > kMinEndSize + kMaxCommentSize ? data.size() - kMinEndSize - kMaxCommentSize : 0;

    for (ca::usize i = data.size() - kMinEndSize + 1; i-- > searchStart;) {
        if (read_u32(&data[i]) == kEndSig && is_valid_end_candidate(data, i)) {
            return Ok(i);
        }
        if (i == 0) {
            break;
        }
    }

    return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "EOCD signature not found"});
}

// 校验候选 ZIP64 EOCD 位置：记录尺寸自洽且不与 locator 重叠。
Result<ca::usize, ZipErrorInfo> resolve_zip64_end_offset(const std::vector<ca::u8>& data, ca::usize locator_offset,
                                                         ca::u64 raw_zip64_end_offset) {
    auto is_valid_zip64_end_offset = [&](ca::usize offset) {
        if (offset > locator_offset || locator_offset - offset < kZip64EndMinSize) {
            return false;
        }
        if (offset + 12 > data.size() || read_u32(data.data() + offset) != kZip64EndSig) {
            return false;
        }

        const ca::u64 record_size = read_u64(data.data() + offset + 4);
        if (record_size < 44 || record_size > std::numeric_limits<ca::u64>::max() - 12) {
            return false;
        }
        const ca::u64 record_length = 12 + record_size;
        return record_length <= locator_offset - offset;
    };

    if (raw_zip64_end_offset <= std::numeric_limits<ca::usize>::max()) {
        const auto rawOffset = static_cast<ca::usize>(raw_zip64_end_offset);
        if (is_valid_zip64_end_offset(rawOffset)) {
            return Ok(rawOffset);
        }
    }

    if (locator_offset < kZip64EndMinSize) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP64 EOCD signature not found"});
    }

    for (ca::usize candidate = locator_offset - kZip64EndMinSize + 1; candidate-- > 0;) {
        if (read_u32(data.data() + candidate) == kZip64EndSig && is_valid_zip64_end_offset(candidate)) {
            return Ok(candidate);
        }
        if (candidate == 0) {
            break;
        }
    }

    return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP64 EOCD signature not found"});
}

struct ResolvedEnd {
    ca::u64 entry_count              = 0;
    ca::u64 central_directory_size   = 0;
    ca::u64 central_directory_offset = 0;
    // CEN 结束锚点：传统 ZIP 为 EOCD 位置，ZIP64 归档为 ZIP64 EOCD 位置。
    ca::u64 central_directory_end = 0;
};

Result<ResolvedEnd, ZipErrorInfo> resolve_end_record(const std::vector<ca::u8>& data, ca::usize eocd_pos) {
    const ca::u8* eocd = data.data() + eocd_pos;
    ResolvedEnd   resolved{};
    resolved.entry_count              = read_u16(eocd + 10);
    resolved.central_directory_size   = read_u32(eocd + 12);
    resolved.central_directory_offset = read_u32(eocd + 16);
    resolved.central_directory_end    = eocd_pos;

    const bool needs_zip64 = resolved.entry_count == 0xFFFFu || resolved.central_directory_size == 0xFFFFFFFFu ||
                             resolved.central_directory_offset == 0xFFFFFFFFu;
    if (!needs_zip64) {
        return Ok(resolved);
    }

    if (eocd_pos < kZip64LocatorSize) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP64 locator not found"});
    }

    const ca::usize locatorOffset = eocd_pos - kZip64LocatorSize;
    const ca::u8*   locator       = data.data() + locatorOffset;
    if (read_u32(locator) != kZip64LocatorSig) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP64 locator not found"});
    }

    const ca::u32 locatorDisk       = read_u32(locator + 4);
    const ca::u64 rawZip64EndOffset = read_u64(locator + 8);
    const ca::u32 totalDisks        = read_u32(locator + 16);
    if (locatorDisk != 0 || totalDisks != 1) {
        return Err(ZipErrorInfo{ZipError::UNSUPPORTED, "Split or multi-disk ZIP not supported"});
    }

    auto zip64EndOffset = resolve_zip64_end_offset(data, locatorOffset, rawZip64EndOffset);
    if (zip64EndOffset.is_err()) {
        return Err(std::move(zip64EndOffset).unwrap_err());
    }
    const ca::usize zip64EndOffsetValue = std::move(zip64EndOffset).unwrap();
    const ca::u8*   zip64End            = data.data() + zip64EndOffsetValue;

    const ca::u64 zip64RecordSize = read_u64(zip64End + 4);
    if (zip64RecordSize < 44 || zip64RecordSize > std::numeric_limits<ca::u64>::max() - 12) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid ZIP64 EOCD size"});
    }
    const ca::u64 zip64RecordLength = 12 + zip64RecordSize;
    if (zip64RecordLength > locatorOffset - zip64EndOffsetValue) {
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "ZIP64 EOCD overlaps locator"});
    }

    const ca::u32 diskNumber                = read_u32(zip64End + 16);
    const ca::u32 centralDirectoryStartDisk = read_u32(zip64End + 20);
    if (diskNumber != 0 || centralDirectoryStartDisk != 0) {
        return Err(ZipErrorInfo{ZipError::UNSUPPORTED, "Split or multi-disk ZIP not supported"});
    }

    resolved.entry_count              = read_u64(zip64End + 32);
    resolved.central_directory_size   = read_u64(zip64End + 40);
    resolved.central_directory_offset = read_u64(zip64End + 48);
    resolved.central_directory_end    = zip64EndOffsetValue;
    return Ok(resolved);
}

}   // anonymous namespace

struct ZipFile::Impl {
    std::vector<ca::u8>                     data;
    std::vector<ZipEntry>                   entries;
    std::unordered_map<std::string, size_t> index;
    bool                                    opened = false;

    void reset() {
        data.clear();
        data.shrink_to_fit();
        entries.clear();
        entries.shrink_to_fit();
        index.clear();
        opened = false;
    }

    Result<void, ZipErrorInfo> parse() {
        auto endResult = find_jvm_style_end(data);
        if (endResult.is_err()) {
            return Err(std::move(endResult).unwrap_err());
        }
        const ca::usize eocdPos = std::move(endResult).unwrap();

        auto resolvedEndResult = resolve_end_record(data, eocdPos);
        if (resolvedEndResult.is_err()) {
            return Err(std::move(resolvedEndResult).unwrap_err());
        }
        const ResolvedEnd resolvedEnd  = std::move(resolvedEndResult).unwrap();
        const ca::u64     entryCount   = resolvedEnd.entry_count;
        const ca::u64     cenSize      = resolvedEnd.central_directory_size;
        const ca::u64     cenOffset    = resolvedEnd.central_directory_offset;
        const ca::u64     cenEndAnchor = resolvedEnd.central_directory_end;
        if (cenSize > std::numeric_limits<ca::usize>::max()) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Central directory size exceeds addressable range"});
        }
        const auto cenSizeBytes = static_cast<ca::usize>(cenSize);

        const bool cenStartInRange     = cenOffset <= data.size();
        ca::usize  cenStart            = cenStartInRange ? static_cast<ca::usize>(cenOffset) : data.size();
        const bool cenEndAnchorInRange = cenEndAnchor <= data.size();
        const bool originalCenValid =
            cenSize == 0 || (cenStartInRange && data.size() - cenStart >= 4 && read_u32(&data[cenStart]) == kCenSig);
        const bool cenExceedsData   = !cenStartInRange || cenSizeBytes > data.size() - cenStart;
        const bool cenExceedsAnchor = cenEndAnchorInRange && cenStart <= cenEndAnchor && cenSize <= cenEndAnchor &&
                                      cenSize > cenEndAnchor - cenStart;
        const bool shouldRecoverBase = cenExceedsData || cenExceedsAnchor || !originalCenValid;
        if (entryCount == 0 && cenSize > 0 && !originalCenValid) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid CEN signature"});
        }
        if (shouldRecoverBase) {
            // Java ZipFile/JVM 语义以最后一条合法 EOCD 为准。
            // 拼接/内嵌 ZIP 的 CEN 偏移相对 ZIP 段自身；ZIP64 归档的 CEN
            // 结束于 ZIP64 EOCD 记录处而非传统 EOCD。
            cenStart = cenEndAnchor >= cenSize ? static_cast<ca::usize>(cenEndAnchor - cenSize) : data.size();
        }

        if (cenStart > data.size() || cenSizeBytes > data.size() - cenStart) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Central directory exceeds file bounds"});
        }

        if (cenSize > 0 && (data.size() - cenStart < 4 || read_u32(&data[cenStart]) != kCenSig)) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid CEN signature"});
        }

        std::ptrdiff_t baseOffset = static_cast<std::ptrdiff_t>(cenStart) - static_cast<std::ptrdiff_t>(cenOffset);

        entries.clear();
        const ca::u64 maxPossibleEntries = cenSize / 46;
        const ca::u64 reserveCount       = std::min(entryCount, maxPossibleEntries);
        entries.reserve(reserveCount > std::numeric_limits<size_t>::max() ? std::numeric_limits<size_t>::max()
                                                                          : static_cast<size_t>(reserveCount));
        index.clear();

        const ca::u8* cen    = &data[cenStart];
        const ca::u8* cenEnd = cen + cenSizeBytes;

        ca::u64 parsedEntries = 0;
        while ((entryCount == 0 && cen < cenEnd) || parsedEntries < entryCount) {
            auto remaining = static_cast<size_t>(cenEnd - cen);
            if (remaining < 46) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "CEN entry exceeds central directory"});
            }
            if (read_u32(cen) != kCenSig) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid CEN signature"});
            }

            ca::u16 nameLen    = read_u16(cen + 28);
            ca::u16 extraLen   = read_u16(cen + 30);
            ca::u16 commentLen = read_u16(cen + 32);

            if (remaining < 46 + static_cast<size_t>(nameLen) + extraLen + commentLen) {
                return Err(
                    ZipErrorInfo{ZipError::INVALID_FORMAT, "CEN entry variable fields exceed central directory"});
            }

            ca::u16    method                       = read_u16(cen + 10);
            ca::u32    crc                          = read_u32(cen + 16);
            ca::u64    csize                        = read_u32(cen + 20);
            ca::u64    usize                        = read_u32(cen + 24);
            ca::u64    relOffset                    = read_u32(cen + 42);
            const bool rawHasZip64UncompressedSize  = usize == 0xFFFFFFFFu;
            const bool rawHasZip64CompressedSize    = csize == 0xFFFFFFFFu;
            const bool rawHasZip64LocalHeaderOffset = relOffset == 0xFFFFFFFFu;

            auto absoluteOffset = static_cast<std::ptrdiff_t>(relOffset) + baseOffset;
            if (absoluteOffset < 0) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "LOC offset before file start"});
            }

            std::string         name(reinterpret_cast<const char*>(cen + 46), nameLen);
            std::vector<ca::u8> extraField(cen + 46 + nameLen, cen + 46 + nameLen + extraLen);

            if (rawHasZip64UncompressedSize) {
                ca::u64 resolvedValue = 0;
                if (!read_zip64_extra_field_value(extraField,
                                                  rawHasZip64UncompressedSize,
                                                  rawHasZip64CompressedSize,
                                                  rawHasZip64LocalHeaderOffset,
                                                  0,
                                                  resolvedValue)) {
                    return Err(
                        ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid ZIP64 extra field for uncompressed size"});
                }
                usize = resolvedValue;
            }
            if (rawHasZip64CompressedSize) {
                ca::u64 resolvedValue = 0;
                if (!read_zip64_extra_field_value(extraField,
                                                  rawHasZip64UncompressedSize,
                                                  rawHasZip64CompressedSize,
                                                  rawHasZip64LocalHeaderOffset,
                                                  1,
                                                  resolvedValue)) {
                    return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid ZIP64 extra field for compressed size"});
                }
                csize = resolvedValue;
            }
            if (rawHasZip64LocalHeaderOffset) {
                ca::u64 resolvedValue = 0;
                if (!read_zip64_extra_field_value(extraField,
                                                  rawHasZip64UncompressedSize,
                                                  rawHasZip64CompressedSize,
                                                  rawHasZip64LocalHeaderOffset,
                                                  2,
                                                  resolvedValue)) {
                    return Err(
                        ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid ZIP64 extra field for local header offset"});
                }
                relOffset      = resolvedValue;
                absoluteOffset = static_cast<std::ptrdiff_t>(relOffset) + baseOffset;
                if (absoluteOffset < 0) {
                    return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "LOC offset before file start"});
                }
            }

            auto checkedCsize = checked_cast_u32(csize, "compressed size");
            if (checkedCsize.is_err()) {
                return Err(std::move(checkedCsize).unwrap_err());
            }
            auto checkedUsize = checked_cast_u32(usize, "uncompressed size");
            if (checkedUsize.is_err()) {
                return Err(std::move(checkedUsize).unwrap_err());
            }
            auto checkedOffset = checked_cast_u32(static_cast<ca::u64>(absoluteOffset), "local header offset");
            if (checkedOffset.is_err()) {
                return Err(std::move(checkedOffset).unwrap_err());
            }

            index[name] = entries.size();
            entries.emplace_back(std::move(name),
                                 checkedCsize.unwrap(),
                                 checkedUsize.unwrap(),
                                 method,
                                 crc,
                                 checkedOffset.unwrap(),
                                 std::move(extraField));

            cen += 46 + nameLen + extraLen + commentLen;
            ++parsedEntries;
        }
        return Ok();
    }

    Result<std::vector<ca::u8>, ZipErrorInfo> read_entry(const ZipEntry& entry) const {
        const ca::usize locOffset = entry.relative_offset();
        if (locOffset > data.size() || data.size() - locOffset < 30) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "LOC header exceeds file bounds"});
        }

        const ca::u8* loc = &data[locOffset];
        if (read_u32(loc) != kLocSig) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Invalid LOC signature"});
        }

        ca::u16 nameLen  = read_u16(loc + 26);
        ca::u16 extraLen = read_u16(loc + 28);

        const ca::usize headerSize = 30 + static_cast<ca::usize>(nameLen) + extraLen;
        if (headerSize > data.size() - locOffset) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "LOC variable fields exceed file bounds"});
        }
        const ca::usize dataOffset     = locOffset + headerSize;
        const ca::usize compressedSize = entry.compressed_size();

        if (compressedSize > data.size() - dataOffset) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "Entry data exceeds file bounds"});
        }

        const ca::u8* compressedData = &data[dataOffset];

        if (entry.is_stored()) {
            std::vector<ca::u8> result(compressedData, compressedData + compressedSize);
            Crc32               crcCalc;
            crcCalc.update(result.data(), result.size());
            if (crcCalc.value() != entry.crc32()) {
                return Err(ZipErrorInfo{ZipError::CRC_MISMATCH, "CRC32 mismatch on stored entry"});
            }
            return Ok(std::move(result));
        }

        if (entry.is_deflated()) {
            auto inflated = inflate_raw(compressedData, compressedSize, entry.uncompressed_size());
            if (inflated.is_err()) {
                return Err(std::move(inflated).unwrap_err());
            }
            auto result = std::move(inflated).unwrap();

            Crc32 crcCalc;
            crcCalc.update(result.data(), result.size());
            if (crcCalc.value() != entry.crc32()) {
                return Err(ZipErrorInfo{ZipError::CRC_MISMATCH, "CRC32 mismatch on deflated entry"});
            }

            return Ok(std::move(result));
        }

        return Err(ZipErrorInfo{ZipError::UNSUPPORTED,
                                "Unsupported compression method: " + std::to_string(entry.compression_method())});
    }
};

ZipFile::ZipFile()
    : impl_(std::make_unique<Impl>()) {}

ZipFile::ZipFile(const std::string& path)
    : impl_(std::make_unique<Impl>()) {
    throw_if_err(open(path));
}

ZipFile::ZipFile(const std::vector<ca::u8>& data)
    : impl_(std::make_unique<Impl>()) {
    throw_if_err(open(data));
}

ZipFile::~ZipFile() = default;

Result<void, ZipErrorInfo> ZipFile::open(const std::string& path) {
    FILE* fp = nullptr;
#ifdef _MSC_VER
    if (::fopen_s(&fp, path.c_str(), "rb") != 0)
        fp = nullptr;
#else
    fp = std::fopen(path.c_str(), "rb");
#endif
    if (!fp) {
        return Err(ZipErrorInfo{ZipError::NOT_FOUND, "Failed to open file: " + path});
    }

    if (::fseek(fp, 0, SEEK_END) != 0) {
        ::fclose(fp);
        return Err(ZipErrorInfo{ZipError::IO_FAILED, "fseek failed"});
    }

    long fileSize = ::ftell(fp);
    if (fileSize < 0) {
        ::fclose(fp);
        return Err(ZipErrorInfo{ZipError::IO_FAILED, "ftell failed"});
    }

    ::rewind(fp);

    impl_->data.resize(static_cast<size_t>(fileSize));
    if (fileSize > 0 &&
        ::fread(impl_->data.data(), 1, static_cast<size_t>(fileSize), fp) != static_cast<size_t>(fileSize)) {
        ::fclose(fp);
        return Err(ZipErrorInfo{ZipError::IO_FAILED, "fread failed"});
    }

    ::fclose(fp);
    auto parsed = impl_->parse();
    if (parsed.is_err()) {
        impl_->reset();
        return Err(std::move(parsed).unwrap_err());
    }
    impl_->opened = true;
    return Ok();
}

Result<void, ZipErrorInfo> ZipFile::open(const std::vector<ca::u8>& data) {
    impl_->data = data;
    auto parsed = impl_->parse();
    if (parsed.is_err()) {
        impl_->reset();
        return Err(std::move(parsed).unwrap_err());
    }
    impl_->opened = true;
    return Ok();
}

bool ZipFile::is_open() const {
    return impl_->opened;
}

void ZipFile::close() {
    impl_->reset();
}

size_t ZipFile::size() const {
    return impl_->entries.size();
}

const ZipEntry* ZipFile::get_entry(const std::string& name) const {
    auto it = impl_->index.find(name);
    if (it == impl_->index.end()) {
        return nullptr;
    }
    return &impl_->entries[it->second];
}

const ZipEntry& ZipFile::get_entry_at(size_t index) const {
    if (index >= impl_->entries.size()) {
        throw std::out_of_range("ZipEntry index out of range");
    }
    return impl_->entries[index];
}

std::vector<std::string> ZipFile::entries() const {
    std::vector<std::string> result;
    result.reserve(impl_->entries.size());
    for (const auto& e : impl_->entries) {
        result.push_back(e.name());
    }
    return result;
}

Result<std::vector<ca::u8>, ZipErrorInfo> ZipFile::read(const std::string& name) const {
    auto it = impl_->index.find(name);
    if (it == impl_->index.end()) {
        return Err(ZipErrorInfo{ZipError::NOT_FOUND, "Entry not found: " + name});
    }
    return impl_->read_entry(impl_->entries[it->second]);
}

Result<std::vector<ca::u8>, ZipErrorInfo> ZipFile::read(const ZipEntry& entry) const {
    return impl_->read_entry(entry);
}

}   // namespace ca::zip
