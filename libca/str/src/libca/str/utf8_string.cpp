//
// @brief 不可变 UTF-8 字符串实现
// @author Canrad
// @date 2026/05/31
//

#include "utf8_string.hpp"
#include "char_util.hpp"
#include "utf8_util.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <mutex>

namespace ca::str {


// ============================================================================
// Utf8StringRef
// ============================================================================

Utf8StringRef::Utf8StringRef() noexcept
    : data_(nullptr)
    , byteLength_(0)
    , length_(0) {}

Utf8StringRef::Utf8StringRef(const u8* data, usize byteLength, usize length) noexcept
    : data_(data)
    , byteLength_(byteLength)
    , length_(length) {}

Utf8StringRef::Utf8StringRef(const Utf8String& str) noexcept
    : data_(str.data())
    , byteLength_(str.byteLength())
    , length_(str.length()) {}

Utf8StringRef Utf8StringRef::fromCStr(const char* cstr) noexcept {
    if (cstr == nullptr) return Utf8StringRef();
    auto len  = std::strlen(cstr);
    auto data = reinterpret_cast<const u8*>(cstr);
    return Utf8StringRef(data, len, utf8CountCodePoints(data, len));
}

usize Utf8StringRef::length() const noexcept {
    return length_;
}

usize Utf8StringRef::byteLength() const noexcept {
    return byteLength_;
}

bool Utf8StringRef::isEmpty() const noexcept {
    return byteLength_ == 0;
}

const u8* Utf8StringRef::data() const noexcept {
    return data_;
}

std::string Utf8StringRef::toStdString() const {
    if (data_ == nullptr || byteLength_ == 0)
        return std::string();
    return std::string(reinterpret_cast<const char*>(data_), byteLength_);
}

u8 Utf8StringRef::byteAt(usize index) const {
    return data_[index];
}

u32 Utf8StringRef::codePointAt(usize index) const {
    usize pos  = 0;
    usize cpIdx = 0;
    while (cpIdx < index && pos < byteLength_) {
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++cpIdx;
    }
    if (pos >= byteLength_)
        return 0;
    return utf8DecodeCodePoint(data_ + pos);
}

Utf8StringRef Utf8StringRef::slice(usize byteStart, usize byteEnd) const {
    if (byteStart >= byteEnd || byteStart >= byteLength_)
        return Utf8StringRef();

    if (byteEnd > byteLength_)
        byteEnd = byteLength_;

    // 从 byteStart 开始扫描，计算区间内的码点个数
    usize pos    = byteStart;
    usize cpCnt  = 0;
    while (pos < byteEnd) {
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++cpCnt;
    }

    return Utf8StringRef(data_ + byteStart, byteEnd - byteStart, cpCnt);
}

Utf8StringRef Utf8StringRef::sliceByCp(usize cpStart, usize cpCount) const {
    if (cpStart >= length_ || cpCount == 0)
        return Utf8StringRef();

    // 扫描到 cpStart
    usize pos = 0;
    for (usize i = 0; i < cpStart; ++i) {
        pos += utf8CodePointBytesSafe(data_[pos]);
    }

    usize startPos = pos;

    // 扫描 cpCount 个码点
    usize actualCount = 0;
    for (usize i = 0; i < cpCount && pos < byteLength_; ++i) {
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++actualCount;
    }

    return Utf8StringRef(data_ + startPos, pos - startPos, actualCount);
}

Utf8String Utf8StringRef::substr(usize cpStart, usize cpCount) const {
    auto ref = sliceByCp(cpStart, cpCount);
    return Utf8String(ref.data(), ref.byteLength());
}

int Utf8StringRef::compare(const Utf8StringRef& other) const noexcept {
    auto cmpLen = byteLength_ < other.byteLength_
                      ? byteLength_
                      : other.byteLength_;

    auto result = std::memcmp(data_, other.data_, cmpLen);
    if (result != 0)
        return result;

    // 前缀相同，较短的字符串小于较长的
    if (byteLength_ < other.byteLength_)
        return -1;
    if (byteLength_ > other.byteLength_)
        return 1;
    return 0;
}

bool Utf8StringRef::equals(const Utf8StringRef& other) const noexcept {
    return byteLength_ == other.byteLength_ &&
           std::memcmp(data_, other.data_, byteLength_) == 0;
}

bool Utf8StringRef::operator==(const Utf8StringRef& other) const noexcept {
    return equals(other);
}

bool Utf8StringRef::operator!=(const Utf8StringRef& other) const noexcept {
    return !equals(other);
}


// ============================================================================
// Utf8String
// ============================================================================

void Utf8String::init(const u8* src, usize byteLen) {
    if (byteLen == 0) {
        data_       = new u8[1]{0};
        byteLength_ = 0;
        length_     = 0;
        return;
    }

    // 校验 UTF-8 合法性并统计码点个数
    usize invalidPos = 0;
    auto  cpCount    = utf8CountCodePoints(src, byteLen, &invalidPos);
    if (cpCount == 0) {
        throw std::runtime_error(
            "Utf8String: invalid UTF-8 sequence at byte position "
            + std::to_string(invalidPos));
    }

    data_ = new u8[byteLen + 1];
    std::memcpy(data_, src, byteLen);
    data_[byteLen] = '\0';

    byteLength_ = byteLen;
    length_     = cpCount;
}

Utf8String::Utf8String() noexcept
    : data_(new u8[1]{0})
    , byteLength_(0)
    , length_(0) {}

Utf8String::Utf8String(const u8* data, usize byteLength)
    : data_(nullptr)
    , byteLength_(0)
    , length_(0) {
    init(data, byteLength);
}

Utf8String::Utf8String(const char* cstr)
    : data_(nullptr)
    , byteLength_(0)
    , length_(0) {
    if (cstr == nullptr) {
        data_       = new u8[1]{0};
        return;
    }
    auto len = std::strlen(cstr);
    init(reinterpret_cast<const u8*>(cstr), len);
}

Utf8String::Utf8String(Utf8String&& other) noexcept
    : data_(other.data_)
    , byteLength_(other.byteLength_)
    , length_(other.length_) {
    other.data_       = nullptr;
    other.byteLength_ = 0;
    other.length_     = 0;
}

Utf8String::~Utf8String() {
    delete[] data_;
    data_ = nullptr;
}

Utf8String& Utf8String::operator=(Utf8String&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_       = other.data_;
        byteLength_ = other.byteLength_;
        length_     = other.length_;
        other.data_       = nullptr;
        other.byteLength_ = 0;
        other.length_     = 0;
    }
    return *this;
}

Utf8String Utf8String::clone() const {
    Utf8String s;
    delete[] s.data_;
    s.data_ = new u8[byteLength_ + 1];
    std::memcpy(s.data_, data_, byteLength_ + 1);
    s.byteLength_ = byteLength_;
    s.length_     = length_;
    return s;
}

Utf8String Utf8String::fromCodePoint(u32 cp) {
    u8 buf[4];
    auto len = utf8EncodeCodePoint(cp, buf);
    if (len == 0) {
        throw std::runtime_error(
            "Utf8String::fromCodePoint: invalid code point U+"
            + std::to_string(cp));
    }
    return Utf8String(buf, len);
}

Utf8String Utf8String::fromUtf8(const u8* data, usize byteLength) {
    return Utf8String(data, byteLength);
}


// ============================================================================
// Utf8StringBuilder
// ============================================================================

Utf8StringBuilder::Utf8StringBuilder() noexcept
    : buffer_(new u8[kDefaultCapacity]), byteLength_(0), capacity_(kDefaultCapacity) {}

Utf8StringBuilder::Utf8StringBuilder(Utf8StringBuilder&& other) noexcept
    : buffer_(other.buffer_), byteLength_(other.byteLength_), capacity_(other.capacity_) {
    other.buffer_ = nullptr; other.byteLength_ = 0; other.capacity_ = 0;
}

Utf8StringBuilder::~Utf8StringBuilder() { delete[] buffer_; }

Utf8StringBuilder& Utf8StringBuilder::operator=(Utf8StringBuilder&& other) noexcept {
    if (this != &other) {
        delete[] buffer_;
        buffer_ = other.buffer_; byteLength_ = other.byteLength_; capacity_ = other.capacity_;
        other.buffer_ = nullptr; other.byteLength_ = 0; other.capacity_ = 0;
    }
    return *this;
}

void Utf8StringBuilder::grow(usize minCapacity) {
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity) newCap = minCapacity;
    if (newCap < kDefaultCapacity) newCap = kDefaultCapacity;
    auto newBuf = new u8[newCap];
    if (byteLength_ > 0) std::memcpy(newBuf, buffer_, byteLength_);
    delete[] buffer_;
    buffer_ = newBuf; capacity_ = newCap;
}

Utf8StringBuilder& Utf8StringBuilder::append(const Utf8StringRef& str) {
    return append(str.data(), str.byteLength());
}

Utf8StringBuilder& Utf8StringBuilder::append(const Utf8String& str) {
    return append(str.data(), str.byteLength());
}

Utf8StringBuilder& Utf8StringBuilder::append(const char* cstr) {
    if (cstr == nullptr) return *this;
    return append(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringBuilder& Utf8StringBuilder::append(const u8* data, usize byteLen) {
    if (byteLen == 0) return *this;
    auto needed = byteLength_ + byteLen;
    if (needed > capacity_) grow(needed);
    std::memcpy(buffer_ + byteLength_, data, byteLen);
    byteLength_ = needed;
    return *this;
}

bool Utf8StringBuilder::appendCodePoint(u32 cp) {
    u8 buf[4];
    auto len = utf8EncodeCodePoint(cp, buf);
    if (len == 0) return false;
    append(buf, len);
    return true;
}

void Utf8StringBuilder::reserve(usize byteCapacity) {
    if (byteCapacity > capacity_) {
        auto newBuf = new u8[byteCapacity];
        if (byteLength_ > 0) std::memcpy(newBuf, buffer_, byteLength_);
        delete[] buffer_;
        buffer_ = newBuf; capacity_ = byteCapacity;
    }
}

usize Utf8StringBuilder::capacity() const noexcept { return capacity_; }
usize Utf8StringBuilder::byteLength() const noexcept { return byteLength_; }
bool Utf8StringBuilder::isEmpty() const noexcept { return byteLength_ == 0; }
void Utf8StringBuilder::clear() noexcept { byteLength_ = 0; }

Utf8String Utf8StringBuilder::build() const {
    return Utf8String(buffer_, byteLength_);
}

Utf8String Utf8StringBuilder::buildOrEmpty() const noexcept {
    if (!utf8IsValid(buffer_, byteLength_))
        return Utf8String();
    return Utf8String(buffer_, byteLength_);
}


usize Utf8String::length() const noexcept {
    return length_;
}

usize Utf8String::byteLength() const noexcept {
    return byteLength_;
}

bool Utf8String::isEmpty() const noexcept {
    return byteLength_ == 0;
}

const u8* Utf8String::data() const noexcept {
    return data_;
}

const char* Utf8String::cStr() const noexcept {
    return reinterpret_cast<const char*>(data_);
}

std::string Utf8String::toStdString() const {
    return std::string(reinterpret_cast<const char*>(data_), byteLength_);
}

u8 Utf8String::byteAt(usize index) const {
    return data_[index];
}

u32 Utf8String::codePointAt(usize index) const {
    usize pos   = 0;
    usize cpIdx = 0;
    while (cpIdx < index && pos < byteLength_) {
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++cpIdx;
    }
    if (pos >= byteLength_)
        return 0;
    return utf8DecodeCodePoint(data_ + pos);
}

Utf8StringRef Utf8String::ref() const noexcept {
    return Utf8StringRef(data_, byteLength_, length_);
}

Utf8StringRef Utf8String::slice(usize byteStart, usize byteEnd) const {
    return ref().slice(byteStart, byteEnd);
}

Utf8StringRef Utf8String::sliceByCp(usize cpStart, usize cpCount) const {
    return ref().sliceByCp(cpStart, cpCount);
}

Utf8String Utf8String::substr(usize cpStart, usize cpCount) const {
    return ref().substr(cpStart, cpCount);
}

int Utf8String::compare(const Utf8StringRef& other) const noexcept {
    return ref().compare(other);
}

int Utf8String::compare(const Utf8String& other) const noexcept {
    return ref().compare(other.ref());
}

bool Utf8String::equals(const Utf8StringRef& other) const noexcept {
    return ref().equals(other);
}

bool Utf8String::operator==(const Utf8String& other) const noexcept {
    return ref().equals(other.ref());
}

bool Utf8String::operator==(const Utf8StringRef& other) const noexcept {
    return ref().equals(other);
}

bool Utf8String::operator!=(const Utf8String& other) const noexcept {
    return !ref().equals(other.ref());
}

bool Utf8String::operator!=(const Utf8StringRef& other) const noexcept {
    return !ref().equals(other);
}


// ============================================================================
// Utf8StringRef — 新增操作
// ============================================================================

bool Utf8StringRef::startsWith(const Utf8StringRef& prefix) const noexcept {
    if (prefix.byteLength_ > byteLength_) return false;
    return std::memcmp(data_, prefix.data_, prefix.byteLength_) == 0;
}

bool Utf8StringRef::endsWith(const Utf8StringRef& suffix) const noexcept {
    if (suffix.byteLength_ > byteLength_) return false;
    return std::memcmp(data_ + byteLength_ - suffix.byteLength_,
                       suffix.data_, suffix.byteLength_) == 0;
}

Utf8StringRef Utf8StringRef::trimStart() const noexcept {
    usize pos = 0;
    while (pos < byteLength_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        if (!ch.isSpace()) break;
        pos += utf8CodePointBytesSafe(data_[pos]);
    }
    return slice(pos, byteLength_);
}

Utf8StringRef Utf8StringRef::trimEnd() const noexcept {
    usize pos = byteLength_;
    while (pos > 0) {
        usize prev = pos - 1;
        while (prev > 0 && (data_[prev] & 0xC0) == 0x80)
            --prev;
        auto ch = Utf8Char::fromRaw(data_ + prev);
        if (!ch.isSpace()) break;
        pos = prev;
    }
    return slice(0, pos);
}

Utf8StringRef Utf8StringRef::trim() const noexcept {
    auto r = trimStart();
    return r.trimEnd();
}

std::vector<Utf8StringRef> Utf8StringRef::split(const Utf8StringRef& delimiter) const {
    std::vector<Utf8StringRef> result;
    if (isEmpty()) return result;
    if (delimiter.isEmpty()) { result.push_back(*this); return result; }

    usize start = 0;
    while (start <= byteLength_) {
        usize remain = byteLength_ - start;
        usize found = usize(-1);
        // 查找分隔符
        for (usize i = start; i + delimiter.byteLength_ <= byteLength_; ) {
            if (std::memcmp(data_ + i, delimiter.data_, delimiter.byteLength_) == 0) {
                found = i;
                break;
            }
            i += utf8CodePointBytesSafe(data_[i]);
        }
        if (found == usize(-1)) {
            // 未找到，剩余整个作为最后一段
            auto ref = slice(start, byteLength_);
            result.push_back(ref);
            break;
        }
        auto ref = slice(start, found);
        result.push_back(ref);
        start = found + delimiter.byteLength_;
    }
    return result;
}

Utf8String Utf8StringRef::toLower() const {
    Utf8StringBuilder b;
    usize pos = 0;
    while (pos < byteLength_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        b.appendCodePoint(ch.toLower().codePoint());
        pos += utf8CodePointBytesSafe(data_[pos]);
    }
    return b.build();
}

Utf8String Utf8StringRef::toUpper() const {
    Utf8StringBuilder b;
    usize pos = 0;
    while (pos < byteLength_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        b.appendCodePoint(ch.toUpper().codePoint());
        pos += utf8CodePointBytesSafe(data_[pos]);
    }
    return b.build();
}

Utf8String Utf8StringRef::replaceAll(const Utf8StringRef& from, const Utf8StringRef& to) const {
    if (from.isEmpty()) return Utf8String(data_, byteLength_);
    auto parts = split(from);
    // 用 to 连接各部分
    Utf8StringBuilder b;
    for (usize i = 0; i < parts.size(); ++i) {
        if (i > 0) b.append(to);
        b.append(parts[i]);
    }
    return b.build();
}


// ============================================================================
// Utf8StringRef — 查找
// ============================================================================

usize Utf8StringRef::indexOf(const Utf8StringRef& needle) const noexcept {
    return indexOf(needle, 0);
}

usize Utf8StringRef::indexOf(const Utf8StringRef& needle, usize startCp) const noexcept {
    if (needle.isEmpty() || needle.byteLength_ > byteLength_)
        return npos;

    usize pos = 0;
    for (usize i = 0; i < startCp && pos < byteLength_; ++i) {
        pos += utf8CodePointBytesSafe(data_[pos]);
    }

    usize cpIdx = startCp;
    while (pos + needle.byteLength_ <= byteLength_) {
        if (std::memcmp(data_ + pos, needle.data_, needle.byteLength_) == 0)
            return cpIdx;
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++cpIdx;
    }
    return npos;
}

usize Utf8StringRef::indexOf(u32 codePoint) const noexcept {
    return indexOf(codePoint, 0);
}

usize Utf8StringRef::indexOf(u32 codePoint, usize startCp) const noexcept {
    usize pos = 0;
    for (usize i = 0; i < startCp && pos < byteLength_; ++i) {
        pos += utf8CodePointBytesSafe(data_[pos]);
    }
    usize cpIdx = startCp;
    while (pos < byteLength_) {
        if (utf8DecodeCodePoint(data_ + pos) == codePoint)
            return cpIdx;
        pos += utf8CodePointBytesSafe(data_[pos]);
        ++cpIdx;
    }
    return npos;
}

bool Utf8StringRef::contains(const Utf8StringRef& needle) const noexcept {
    return indexOf(needle) != npos;
}


// ============================================================================
// Utf8StringRef — 迭代器
// ============================================================================

Utf8Iterator Utf8StringRef::begin() const noexcept {
    return Utf8Iterator(data_, data_ + byteLength_);
}

Utf8Iterator Utf8StringRef::end() const noexcept {
    return Utf8Iterator(data_ + byteLength_, data_ + byteLength_);
}


// ============================================================================
// Utf8String — 新增操作（委托给 ref）
// ============================================================================

bool Utf8String::startsWith(const Utf8StringRef& prefix) const noexcept { return ref().startsWith(prefix); }
bool Utf8String::endsWith(const Utf8StringRef& suffix) const noexcept   { return ref().endsWith(suffix); }
Utf8StringRef Utf8String::trim() const noexcept       { return ref().trim(); }
Utf8StringRef Utf8String::trimStart() const noexcept   { return ref().trimStart(); }
Utf8StringRef Utf8String::trimEnd() const noexcept     { return ref().trimEnd(); }
std::vector<Utf8StringRef> Utf8String::split(const Utf8StringRef& d) const { return ref().split(d); }
Utf8String Utf8String::toLower() const    { return ref().toLower(); }
Utf8String Utf8String::toUpper() const    { return ref().toUpper(); }
Utf8String Utf8String::replaceAll(const Utf8StringRef& from, const Utf8StringRef& to) const {
    return ref().replaceAll(from, to);
}

usize Utf8String::indexOf(const Utf8StringRef& needle) const noexcept {
    return ref().indexOf(needle);
}

usize Utf8String::indexOf(const Utf8StringRef& needle, usize startCp) const noexcept {
    return ref().indexOf(needle, startCp);
}

usize Utf8String::indexOf(u32 codePoint) const noexcept {
    return ref().indexOf(codePoint);
}

usize Utf8String::indexOf(u32 codePoint, usize startCp) const noexcept {
    return ref().indexOf(codePoint, startCp);
}

bool Utf8String::contains(const Utf8StringRef& needle) const noexcept {
    return ref().contains(needle);
}

Utf8Iterator Utf8String::begin() const noexcept {
    return ref().begin();
}

Utf8Iterator Utf8String::end() const noexcept {
    return ref().end();
}


// ============================================================================
// 非成员比较运算符
// ============================================================================

bool operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept {
    return lhs.equals(rhs.ref());
}

bool operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept {
    return !lhs.equals(rhs.ref());
}


// ============================================================================
// 自由函数
// ============================================================================

std::vector<Utf8StringRef> split(const Utf8StringRef& str,
                                 const Utf8StringRef& delimiter) {
    return str.split(delimiter);
}

Utf8String join(const std::vector<Utf8StringRef>& parts,
                const Utf8StringRef& separator) {
    Utf8StringBuilder b;
    for (usize i = 0; i < parts.size(); ++i) {
        if (i > 0) b.append(separator);
        b.append(parts[i]);
    }
    return b.build();
}


// ============================================================================
// 流输出
// ============================================================================

std::ostream& operator<<(std::ostream& os, const Utf8StringRef& s) {
    if (s.data()) {
        os.write(reinterpret_cast<const char*>(s.data()),
                 static_cast<std::streamsize>(s.byteLength()));
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Utf8String& s) {
    os << s.ref();
    return os;
}

struct CacheData {
    std::mutex mutex;
    // key: 字符串的内存地址（字面量地址全局唯一且不变）
    std::unordered_map<const char*, ZUtf8StringRef> map;
};

static CacheData& getCache() {
    static CacheData cache;
    return cache;
}

ZUtf8StringRef ZUtf8StringRef::from_static(const char* cstr)
{
    if (!cstr) return ZUtf8StringRef(nullptr, 0, 0);
        
    auto& inst = getCache();
    std::lock_guard<std::mutex> lock(inst.mutex);
    
    auto it = inst.map.find(cstr);
    if (it != inst.map.end()) {
        return it->second; // 命中缓存
    }
    
    // 未命中，计算并加入缓存
    usize len = std::strlen(cstr);
    usize cp = utf8CountCodePoints(reinterpret_cast<const u8*>(cstr), len);
    ZUtf8StringRef ref(reinterpret_cast<const u8*>(cstr), len, cp);
    inst.map[cstr] = ref;
    return ref;
}

ZUtf8StringRef ZUtf8StringRef::from_utf8_string(const Utf8String& s) {
    return ZUtf8StringRef(s.data(), s.byteLength(), s.length());
}

ZUtf8StringRef ZUtf8StringRef::from_std_string(const std::string& s)
{
    return ZUtf8StringRef(reinterpret_cast<const u8*>(s.data()), s.length(), s.length());
}

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::Utf8String>::operator()(
    const ca::str::Utf8String& s) const noexcept {
    auto data = s.data();
    auto len  = s.byteLength();
    // FNV-1a hash
    size_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

size_t hash<ca::str::Utf8StringRef>::operator()(
    const ca::str::Utf8StringRef& s) const noexcept {
    auto data = s.data();
    auto len  = s.byteLength();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
