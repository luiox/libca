//
// @brief 不可变 UTF-8 字符串实现
// @author Canrad
// @date 2026/05/31
//

#include <libca/str/utf8_string.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

namespace ca::str {

// ============================================================================
// UTF-8 编解码工具函数
// ============================================================================

usize utf8CodePointBytes(u8 firstByte) noexcept {
    if ((firstByte & 0x80) == 0)
        return 1;   // 0xxxxxxx
    if ((firstByte & 0xE0) == 0xC0)
        return 2;   // 110xxxxx
    if ((firstByte & 0xF0) == 0xE0)
        return 3;   // 1110xxxx
    if ((firstByte & 0xF8) == 0xF0)
        return 4;   // 11110xxx
    return 0;       // 非法首字节
}

u32 utf8DecodeCodePoint(const u8* bytes) noexcept {
    auto b0 = bytes[0];
    if ((b0 & 0x80) == 0) {
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0) {
        return ((u32)(b0 & 0x1F) << 6) |
               ((u32)(bytes[1] & 0x3F));
    }
    if ((b0 & 0xF0) == 0xE0) {
        return ((u32)(b0 & 0x0F) << 12) |
               ((u32)(bytes[1] & 0x3F) << 6) |
               ((u32)(bytes[2] & 0x3F));
    }
    if ((b0 & 0xF8) == 0xF0) {
        return ((u32)(b0 & 0x07) << 18) |
               ((u32)(bytes[1] & 0x3F) << 12) |
               ((u32)(bytes[2] & 0x3F) << 6) |
               ((u32)(bytes[3] & 0x3F));
    }
    return 0;  // 非法
}

usize utf8EncodeCodePoint(u32 cp, u8* out) noexcept {
    // 排除非法码点：代理项 (U+D800~U+DFFF) 和超出范围的值
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return 0;

    if (cp <= 0x007F) {
        out[0] = static_cast<u8>(cp);
        return 1;
    }
    if (cp <= 0x07FF) {
        out[0] = static_cast<u8>(0xC0 | (cp >> 6));
        out[1] = static_cast<u8>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = static_cast<u8>(0xE0 | (cp >> 12));
        out[1] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
        out[2] = static_cast<u8>(0x80 | (cp & 0x3F));
        return 3;
    }
    // 0x10000 ~ 0x10FFFF
    out[0] = static_cast<u8>(0xF0 | (cp >> 18));
    out[1] = static_cast<u8>(0x80 | ((cp >> 12) & 0x3F));
    out[2] = static_cast<u8>(0x80 | ((cp >> 6) & 0x3F));
    out[3] = static_cast<u8>(0x80 | (cp & 0x3F));
    return 4;
}

usize utf8CountCodePoints(const u8* data, usize byteLength,
                          usize* invalidPos) noexcept {
    usize count = 0;
    usize pos   = 0;

    while (pos < byteLength) {
        auto len = utf8CodePointBytes(data[pos]);
        if (len == 0 || pos + len > byteLength) {
            // 遇到非法序列
            if (invalidPos)
                *invalidPos = pos;
            return 0;
        }
        // 检查后续字节是否都是 10xxxxxx
        for (usize i = 1; i < len; ++i) {
            if ((data[pos + i] & 0xC0) != 0x80) {
                if (invalidPos)
                    *invalidPos = pos + i;
                return 0;
            }
        }
        pos += len;
        ++count;
    }

    return count;
}

bool utf8IsValid(const u8* data, usize byteLength) noexcept {
    usize pos = 0;
    while (pos < byteLength) {
        auto len = utf8CodePointBytes(data[pos]);
        if (len == 0 || pos + len > byteLength)
            return false;
        for (usize i = 1; i < len; ++i) {
            if ((data[pos + i] & 0xC0) != 0x80)
                return false;
        }
        pos += len;
    }
    return true;
}


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

u8 Utf8StringRef::byteAt(usize index) const {
    return data_[index];
}

u32 Utf8StringRef::codePointAt(usize index) const {
    usize pos  = 0;
    usize cpIdx = 0;
    while (cpIdx < index && pos < byteLength_) {
        pos += utf8CodePointBytes(data_[pos]);
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
        pos += utf8CodePointBytes(data_[pos]);
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
        pos += utf8CodePointBytes(data_[pos]);
    }

    usize startPos = pos;

    // 扫描 cpCount 个码点
    usize actualCount = 0;
    for (usize i = 0; i < cpCount && pos < byteLength_; ++i) {
        pos += utf8CodePointBytes(data_[pos]);
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

Utf8String::Utf8String(const Utf8String& other)
    : data_(new u8[other.byteLength_ + 1])
    , byteLength_(other.byteLength_)
    , length_(other.length_) {
    std::memcpy(data_, other.data_, byteLength_ + 1);
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

Utf8String& Utf8String::operator=(const Utf8String& other) {
    if (this != &other) {
        delete[] data_;
        byteLength_ = other.byteLength_;
        length_     = other.length_;
        data_       = new u8[byteLength_ + 1];
        std::memcpy(data_, other.data_, byteLength_ + 1);
    }
    return *this;
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

u8 Utf8String::byteAt(usize index) const {
    return data_[index];
}

u32 Utf8String::codePointAt(usize index) const {
    usize pos   = 0;
    usize cpIdx = 0;
    while (cpIdx < index && pos < byteLength_) {
        pos += utf8CodePointBytes(data_[pos]);
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
// 非成员比较运算符
// ============================================================================

bool operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept {
    return lhs.equals(rhs.ref());
}

bool operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept {
    return !lhs.equals(rhs.ref());
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
