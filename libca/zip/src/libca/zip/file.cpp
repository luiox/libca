#include "libca/zip/file.hpp"

#include <algorithm>
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

ca::u16 read_u16(const ca::u8* p)
{
    return static_cast<ca::u16>(p[0] | (p[1] << 8));
}

ca::u32 read_u32(const ca::u8* p)
{
    return static_cast<ca::u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

ca::u64 read_u64(const ca::u8* p)
{
    return static_cast<ca::u64>(read_u32(p)) | (static_cast<ca::u64>(read_u32(p + 4)) << 32);
}

ca::u32 checked_cast_u32(ca::u64 value, const char* what)
{
    if (value > std::numeric_limits<ca::u32>::max()) {
        throw std::runtime_error(std::string(what) + " exceeds 32-bit limit");
    }
    return static_cast<ca::u32>(value);
}

// 解析 ZIP64 extra field（tag 0x0001）中第 fieldIndex 个出现的字段。
// 哪些字段在场由 has* 标志决定，顺序固定：uncompressed → compressed → offset。
bool read_zip64_extra_field_value(const std::vector<ca::u8>& extra_field,
                                  bool has_uncompressed_size, bool has_compressed_size,
                                  bool has_local_header_offset, int field_index, ca::u64& out_value)
{
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

bool is_valid_end_candidate(const std::vector<ca::u8>& data, ca::usize eocd_pos)
{
    if (eocd_pos + kMinEndSize > data.size()) {
        return false;
    }
    const ca::u8* eocd       = &data[eocd_pos];
    ca::u16       commentLen = read_u16(eocd + 20);
    return eocd_pos + kMinEndSize + commentLen == data.size();
}

// JVM ZipFile 语义：从尾部向前找最后一条合法 EOCD（注释长度自洽）。
ca::usize find_jvm_style_end(const std::vector<ca::u8>& data)
{
    if (data.size() < kMinEndSize) {
        throw std::runtime_error("ZIP file too small");
    }

    ca::usize searchStart = data.size() > kMinEndSize + kMaxCommentSize
                                ? data.size() - kMinEndSize - kMaxCommentSize
                                : 0;

    for (ca::usize i = data.size() - kMinEndSize + 1; i-- > searchStart;) {
        if (read_u32(&data[i]) == kEndSig && is_valid_end_candidate(data, i)) {
            return i;
        }
        if (i == 0) {
            break;
        }
    }

    throw std::runtime_error("EOCD signature not found");
}

// 校验候选 ZIP64 EOCD 位置：记录尺寸自洽且不与 locator 重叠。
ca::usize resolve_zip64_end_offset(const std::vector<ca::u8>& data, ca::usize locator_offset,
                                   ca::u64 raw_zip64_end_offset)
{
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
            return rawOffset;
        }
    }

    if (locator_offset < kZip64EndMinSize) {
        throw std::runtime_error("ZIP64 EOCD signature not found");
    }

    for (ca::usize candidate = locator_offset - kZip64EndMinSize + 1; candidate-- > 0;) {
        if (read_u32(data.data() + candidate) == kZip64EndSig &&
            is_valid_zip64_end_offset(candidate)) {
            return candidate;
        }
        if (candidate == 0) {
            break;
        }
    }

    throw std::runtime_error("ZIP64 EOCD signature not found");
}

struct ResolvedEnd
{
    ca::u64 entry_count              = 0;
    ca::u64 central_directory_size   = 0;
    ca::u64 central_directory_offset = 0;
    // CEN 结束锚点：传统 ZIP 为 EOCD 位置，ZIP64 归档为 ZIP64 EOCD 位置。
    ca::u64 central_directory_end = 0;
};

ResolvedEnd resolve_end_record(const std::vector<ca::u8>& data, ca::usize eocd_pos)
{
    const ca::u8* eocd = data.data() + eocd_pos;
    ResolvedEnd   resolved{};
    resolved.entry_count              = read_u16(eocd + 10);
    resolved.central_directory_size   = read_u32(eocd + 12);
    resolved.central_directory_offset = read_u32(eocd + 16);
    resolved.central_directory_end    = eocd_pos;

    const bool needs_zip64 = resolved.entry_count == 0xFFFFu ||
                             resolved.central_directory_size == 0xFFFFFFFFu ||
                             resolved.central_directory_offset == 0xFFFFFFFFu;
    if (!needs_zip64) {
        return resolved;
    }

    if (eocd_pos < kZip64LocatorSize) {
        throw std::runtime_error("ZIP64 locator not found");
    }

    const ca::usize locatorOffset = eocd_pos - kZip64LocatorSize;
    const ca::u8*   locator       = data.data() + locatorOffset;
    if (read_u32(locator) != kZip64LocatorSig) {
        throw std::runtime_error("ZIP64 locator not found");
    }

    const ca::u32 locatorDisk       = read_u32(locator + 4);
    const ca::u64 rawZip64EndOffset = read_u64(locator + 8);
    const ca::u32 totalDisks        = read_u32(locator + 16);
    if (locatorDisk != 0 || totalDisks != 1) {
        throw std::runtime_error("Split or multi-disk ZIP not supported");
    }

    const ca::usize zip64EndOffset =
        resolve_zip64_end_offset(data, locatorOffset, rawZip64EndOffset);
    const ca::u8* zip64End = data.data() + zip64EndOffset;

    const ca::u64 zip64RecordSize = read_u64(zip64End + 4);
    if (zip64RecordSize < 44 || zip64RecordSize > std::numeric_limits<ca::u64>::max() - 12) {
        throw std::runtime_error("Invalid ZIP64 EOCD size");
    }
    const ca::u64 zip64RecordLength = 12 + zip64RecordSize;
    if (zip64RecordLength > locatorOffset - zip64EndOffset) {
        throw std::runtime_error("ZIP64 EOCD overlaps locator");
    }

    const ca::u32 diskNumber                = read_u32(zip64End + 16);
    const ca::u32 centralDirectoryStartDisk = read_u32(zip64End + 20);
    if (diskNumber != 0 || centralDirectoryStartDisk != 0) {
        throw std::runtime_error("Split or multi-disk ZIP not supported");
    }

    resolved.entry_count              = read_u64(zip64End + 32);
    resolved.central_directory_size   = read_u64(zip64End + 40);
    resolved.central_directory_offset = read_u64(zip64End + 48);
    resolved.central_directory_end    = zip64EndOffset;
    return resolved;
}

}   // anonymous namespace

struct ZipFile::Impl
{
    std::vector<ca::u8>                     data;
    std::vector<ZipEntry>                   entries;
    std::unordered_map<std::string, size_t> index;
    bool                                    opened = false;

    void parse()
    {
        const ca::usize eocdPos = find_jvm_style_end(data);

        const auto    resolvedEnd  = resolve_end_record(data, eocdPos);
        const ca::u64 entryCount   = resolvedEnd.entry_count;
        const ca::u64 cenSize      = resolvedEnd.central_directory_size;
        const ca::u64 cenOffset    = resolvedEnd.central_directory_offset;
        const ca::u64 cenEndAnchor = resolvedEnd.central_directory_end;
        if (cenSize > std::numeric_limits<ca::usize>::max()) {
            throw std::runtime_error("Central directory size exceeds addressable range");
        }
        const auto cenSizeBytes = static_cast<ca::usize>(cenSize);

        const bool cenStartInRange = cenOffset <= data.size();
        ca::usize  cenStart = cenStartInRange ? static_cast<ca::usize>(cenOffset) : data.size();
        const bool cenEndAnchorInRange = cenEndAnchor <= data.size();
        const bool originalCenValid =
            cenSize == 0 || (cenStartInRange && data.size() - cenStart >= 4 &&
                             read_u32(&data[cenStart]) == kCenSig);
        const bool cenExceedsData   = !cenStartInRange || cenSizeBytes > data.size() - cenStart;
        const bool cenExceedsAnchor = cenEndAnchorInRange && cenStart <= cenEndAnchor &&
                                      cenSize <= cenEndAnchor && cenSize > cenEndAnchor - cenStart;
        const bool shouldRecoverBase = cenExceedsData || cenExceedsAnchor || !originalCenValid;
        if (entryCount == 0 && cenSize > 0 && !originalCenValid) {
            throw std::runtime_error("Invalid CEN signature");
        }
        if (shouldRecoverBase) {
            // Java ZipFile/JVM 语义以最后一条合法 EOCD 为准。
            // 拼接/内嵌 ZIP 的 CEN 偏移相对 ZIP 段自身；ZIP64 归档的 CEN
            // 结束于 ZIP64 EOCD 记录处而非传统 EOCD。
            cenStart = cenEndAnchor >= cenSize ? static_cast<ca::usize>(cenEndAnchor - cenSize)
                                               : data.size();
        }

        if (cenStart > data.size() || cenSizeBytes > data.size() - cenStart) {
            throw std::runtime_error("Central directory exceeds file bounds");
        }

        if (cenSize > 0 && (data.size() - cenStart < 4 || read_u32(&data[cenStart]) != kCenSig)) {
            throw std::runtime_error("Invalid CEN signature");
        }

        std::ptrdiff_t baseOffset =
            static_cast<std::ptrdiff_t>(cenStart) - static_cast<std::ptrdiff_t>(cenOffset);

        entries.clear();
        const ca::u64 maxPossibleEntries = cenSize / 46;
        const ca::u64 reserveCount       = std::min(entryCount, maxPossibleEntries);
        entries.reserve(reserveCount > std::numeric_limits<size_t>::max()
                            ? std::numeric_limits<size_t>::max()
                            : static_cast<size_t>(reserveCount));
        index.clear();

        const ca::u8* cen    = &data[cenStart];
        const ca::u8* cenEnd = cen + cenSizeBytes;

        ca::u64 parsedEntries = 0;
        while ((entryCount == 0 && cen < cenEnd) || parsedEntries < entryCount) {
            auto remaining = static_cast<size_t>(cenEnd - cen);
            if (remaining < 46) {
                throw std::runtime_error("CEN entry exceeds central directory");
            }
            if (read_u32(cen) != kCenSig) {
                throw std::runtime_error("Invalid CEN signature");
            }

            ca::u16 nameLen    = read_u16(cen + 28);
            ca::u16 extraLen   = read_u16(cen + 30);
            ca::u16 commentLen = read_u16(cen + 32);

            if (remaining < 46 + static_cast<size_t>(nameLen) + extraLen + commentLen) {
                throw std::runtime_error("CEN entry variable fields exceed central directory");
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
                throw std::runtime_error("LOC offset before file start");
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
                    throw std::runtime_error("Invalid ZIP64 extra field for uncompressed size");
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
                    throw std::runtime_error("Invalid ZIP64 extra field for compressed size");
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
                    throw std::runtime_error("Invalid ZIP64 extra field for local header offset");
                }
                relOffset      = resolvedValue;
                absoluteOffset = static_cast<std::ptrdiff_t>(relOffset) + baseOffset;
                if (absoluteOffset < 0) {
                    throw std::runtime_error("LOC offset before file start");
                }
            }

            index[name] = entries.size();
            entries.emplace_back(
                std::move(name),
                checked_cast_u32(csize, "compressed size"),
                checked_cast_u32(usize, "uncompressed size"),
                method,
                crc,
                checked_cast_u32(static_cast<ca::u64>(absoluteOffset), "local header offset"),
                std::move(extraField));

            cen += 46 + nameLen + extraLen + commentLen;
            ++parsedEntries;
        }
    }

    std::vector<ca::u8> read_entry(const ZipEntry& entry) const
    {
        const ca::usize locOffset = entry.relative_offset();
        if (locOffset > data.size() || data.size() - locOffset < 30) {
            throw std::runtime_error("LOC header exceeds file bounds");
        }

        const ca::u8* loc = &data[locOffset];
        if (read_u32(loc) != kLocSig) {
            throw std::runtime_error("Invalid LOC signature");
        }

        ca::u16 nameLen  = read_u16(loc + 26);
        ca::u16 extraLen = read_u16(loc + 28);

        const ca::usize headerSize = 30 + static_cast<ca::usize>(nameLen) + extraLen;
        if (headerSize > data.size() - locOffset) {
            throw std::runtime_error("LOC variable fields exceed file bounds");
        }
        const ca::usize dataOffset     = locOffset + headerSize;
        const ca::usize compressedSize = entry.compressed_size();

        if (compressedSize > data.size() - dataOffset) {
            throw std::runtime_error("Entry data exceeds file bounds");
        }

        const ca::u8* compressedData = &data[dataOffset];

        if (entry.is_stored()) {
            std::vector<ca::u8> result(compressedData, compressedData + compressedSize);
            Crc32               crcCalc;
            crcCalc.update(result.data(), result.size());
            if (crcCalc.value() != entry.crc32()) {
                throw std::runtime_error("CRC32 mismatch on stored entry");
            }
            return result;
        }

        if (entry.is_deflated()) {
            auto result = inflate_raw(compressedData, compressedSize, entry.uncompressed_size());

            Crc32 crcCalc;
            crcCalc.update(result.data(), result.size());
            if (crcCalc.value() != entry.crc32()) {
                throw std::runtime_error("CRC32 mismatch on deflated entry");
            }

            return result;
        }

        throw std::runtime_error("Unsupported compression method: " +
                                 std::to_string(entry.compression_method()));
    }
};

ZipFile::ZipFile()
    : impl_(std::make_unique<Impl>())
{}

ZipFile::ZipFile(const std::string& path)
    : impl_(std::make_unique<Impl>())
{
    open(path);
}

ZipFile::ZipFile(const std::vector<ca::u8>& data)
    : impl_(std::make_unique<Impl>())
{
    open(data);
}

ZipFile::~ZipFile() = default;

void ZipFile::open(const std::string& path)
{
    FILE* fp = nullptr;
#ifdef _MSC_VER
    if (::fopen_s(&fp, path.c_str(), "rb") != 0)
        fp = nullptr;
#else
    fp = std::fopen(path.c_str(), "rb");
#endif
    if (!fp) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    if (::fseek(fp, 0, SEEK_END) != 0) {
        ::fclose(fp);
        throw std::runtime_error("fseek failed");
    }

    long fileSize = ::ftell(fp);
    if (fileSize < 0) {
        ::fclose(fp);
        throw std::runtime_error("ftell failed");
    }

    ::rewind(fp);

    impl_->data.resize(static_cast<size_t>(fileSize));
    if (fileSize > 0 && ::fread(impl_->data.data(), 1, static_cast<size_t>(fileSize), fp) !=
                            static_cast<size_t>(fileSize)) {
        ::fclose(fp);
        throw std::runtime_error("fread failed");
    }

    ::fclose(fp);
    impl_->parse();
    impl_->opened = true;
}

void ZipFile::open(const std::vector<ca::u8>& data)
{
    impl_->data = data;
    impl_->parse();
    impl_->opened = true;
}

bool ZipFile::is_open() const
{
    return impl_->opened;
}

void ZipFile::close()
{
    impl_->data.clear();
    impl_->data.shrink_to_fit();
    impl_->entries.clear();
    impl_->entries.shrink_to_fit();
    impl_->index.clear();
    impl_->opened = false;
}

size_t ZipFile::size() const
{
    return impl_->entries.size();
}

const ZipEntry* ZipFile::get_entry(const std::string& name) const
{
    auto it = impl_->index.find(name);
    if (it == impl_->index.end()) {
        return nullptr;
    }
    return &impl_->entries[it->second];
}

const ZipEntry& ZipFile::get_entry_at(size_t index) const
{
    if (index >= impl_->entries.size()) {
        throw std::out_of_range("ZipEntry index out of range");
    }
    return impl_->entries[index];
}

std::vector<std::string> ZipFile::entries() const
{
    std::vector<std::string> result;
    result.reserve(impl_->entries.size());
    for (const auto& e : impl_->entries) {
        result.push_back(e.name());
    }
    return result;
}

std::vector<ca::u8> ZipFile::read(const std::string& name) const
{
    auto it = impl_->index.find(name);
    if (it == impl_->index.end()) {
        throw std::runtime_error("Entry not found: " + name);
    }
    return impl_->read_entry(impl_->entries[it->second]);
}

std::vector<ca::u8> ZipFile::read(const ZipEntry& entry) const
{
    return impl_->read_entry(entry);
}

}   // namespace ca::zip
