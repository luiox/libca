//
// @brief WString / WStringRef / WStringBuilder 实现
// @author Canrad
// @date 2026/05/31
//

#include "wstring.hpp"

#include <cstring>
#include <cwchar>

namespace ca::str {

// ============================================================================
// WStringRef
// ============================================================================

WStringRef::WStringRef() noexcept : data_(nullptr), length_(0) {}
WStringRef::WStringRef(const wchar_t* data, usize length) noexcept : data_(data), length_(length) {}
WStringRef::WStringRef(const WString& str) noexcept : data_(str.data()), length_(str.length()) {}

usize WStringRef::length() const noexcept { return length_; }
bool WStringRef::isEmpty() const noexcept { return length_ == 0; }
const wchar_t* WStringRef::data() const noexcept { return data_; }
wchar_t WStringRef::at(usize index) const { return data_[index]; }

WStringRef WStringRef::slice(usize start, usize end) const {
    if (start >= end || start >= length_) return WStringRef();
    if (end > length_) end = length_;
    return WStringRef(data_ + start, end - start);
}

WString WStringRef::substr(usize start, usize count) const {
    auto ref = slice(start, start + count);
    return WString(ref.data(), ref.length());
}

int WStringRef::compare(const WStringRef& other) const noexcept {
    auto cmpLen = length_ < other.length_ ? length_ : other.length_;
    auto result = std::wmemcmp(data_, other.data_, cmpLen);
    if (result != 0) return result;
    if (length_ < other.length_) return -1;
    if (length_ > other.length_) return 1;
    return 0;
}

bool WStringRef::equals(const WStringRef& other) const noexcept {
    return length_ == other.length_ && std::wmemcmp(data_, other.data_, length_) == 0;
}

bool WStringRef::operator==(const WStringRef& other) const noexcept { return equals(other); }
bool WStringRef::operator!=(const WStringRef& other) const noexcept { return !equals(other); }


// ============================================================================
// WString
// ============================================================================

void WString::init(const wchar_t* src, usize len) {
    data_ = new wchar_t[len + 1];
    std::wmemcpy(data_, src, len);
    data_[len] = L'\0';
    length_ = len;
}

WString::WString() noexcept : data_(new wchar_t[1]{0}), length_(0) {}

WString::WString(const wchar_t* data, usize length) : data_(nullptr), length_(0) {
    if (data == nullptr || length == 0) { data_ = new wchar_t[1]{0}; length_ = 0; return; }
    init(data, length);
}

WString::WString(const wchar_t* wstr) : data_(nullptr), length_(0) {
    if (wstr == nullptr) { data_ = new wchar_t[1]{0}; return; }
    init(wstr, std::wcslen(wstr));
}

WString::WString(WString&& other) noexcept : data_(other.data_), length_(other.length_) {
    other.data_ = nullptr; other.length_ = 0;
}

WString::~WString() { delete[] data_; }

WString& WString::operator=(WString&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_; length_ = other.length_;
        other.data_ = nullptr; other.length_ = 0;
    }
    return *this;
}

WString WString::clone() const {
    WString w;
    delete[] w.data_;
    w.data_ = new wchar_t[length_ + 1];
    std::wmemcpy(w.data_, data_, length_ + 1);
    w.length_ = length_;
    return w;
}

WString WString::fromWStr(const wchar_t* wstr) { return WString(wstr); }

usize WString::length() const noexcept { return length_; }
bool WString::isEmpty() const noexcept { return length_ == 0; }
const wchar_t* WString::data() const noexcept { return data_; }
const wchar_t* WString::wStr() const noexcept { return data_; }
wchar_t WString::at(usize index) const { return data_[index]; }

WStringRef WString::ref() const noexcept { return WStringRef(data_, length_); }
WStringRef WString::slice(usize start, usize end) const { return ref().slice(start, end); }
WString WString::substr(usize start, usize count) const { return ref().substr(start, count); }

int WString::compare(const WStringRef& other) const noexcept { return ref().compare(other); }
int WString::compare(const WString& other) const noexcept { return ref().compare(other.ref()); }
bool WString::equals(const WStringRef& other) const noexcept { return ref().equals(other); }
bool WString::operator==(const WString& other) const noexcept { return ref().equals(other.ref()); }
bool WString::operator==(const WStringRef& other) const noexcept { return ref().equals(other); }
bool WString::operator!=(const WString& other) const noexcept { return !ref().equals(other.ref()); }
bool WString::operator!=(const WStringRef& other) const noexcept { return !ref().equals(other); }


// ============================================================================
// WStringBuilder
// ============================================================================

WStringBuilder::WStringBuilder() noexcept
    : buffer_(new wchar_t[kDefaultCapacity]), length_(0), capacity_(kDefaultCapacity) {}

WStringBuilder::WStringBuilder(WStringBuilder&& other) noexcept
    : buffer_(other.buffer_), length_(other.length_), capacity_(other.capacity_) {
    other.buffer_ = nullptr; other.length_ = 0; other.capacity_ = 0;
}

WStringBuilder::~WStringBuilder() { delete[] buffer_; }

WStringBuilder& WStringBuilder::operator=(WStringBuilder&& other) noexcept {
    if (this != &other) {
        delete[] buffer_;
        buffer_ = other.buffer_; length_ = other.length_; capacity_ = other.capacity_;
        other.buffer_ = nullptr; other.length_ = 0; other.capacity_ = 0;
    }
    return *this;
}

void WStringBuilder::grow(usize minCapacity) {
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity) newCap = minCapacity;
    if (newCap < kDefaultCapacity) newCap = kDefaultCapacity;
    auto newBuf = new wchar_t[newCap];
    if (length_ > 0) std::wmemcpy(newBuf, buffer_, length_);
    delete[] buffer_;
    buffer_ = newBuf; capacity_ = newCap;
}

WStringBuilder& WStringBuilder::append(const WStringRef& str) { return append(str.data(), str.length()); }
WStringBuilder& WStringBuilder::append(const WString& str) { return append(str.data(), str.length()); }

WStringBuilder& WStringBuilder::append(const wchar_t* wstr) {
    if (wstr == nullptr) return *this;
    return append(wstr, std::wcslen(wstr));
}

WStringBuilder& WStringBuilder::append(const wchar_t* data, usize len) {
    if (len == 0) return *this;
    auto needed = length_ + len;
    if (needed > capacity_) grow(needed);
    std::wmemcpy(buffer_ + length_, data, len);
    length_ = needed;
    return *this;
}

WStringBuilder& WStringBuilder::append(wchar_t ch) { return append(&ch, 1); }

void WStringBuilder::reserve(usize cap) {
    if (cap > capacity_) {
        auto newBuf = new wchar_t[cap];
        if (length_ > 0) std::wmemcpy(newBuf, buffer_, length_);
        delete[] buffer_;
        buffer_ = newBuf; capacity_ = cap;
    }
}

usize WStringBuilder::capacity() const noexcept { return capacity_; }
usize WStringBuilder::length() const noexcept { return length_; }
bool WStringBuilder::isEmpty() const noexcept { return length_ == 0; }
void WStringBuilder::clear() noexcept { length_ = 0; }
WString WStringBuilder::build() const { return WString(buffer_, length_); }


// ============================================================================
// 非成员比较运算符
// ============================================================================

bool operator==(const WStringRef& lhs, const WString& rhs) noexcept { return lhs.equals(rhs.ref()); }
bool operator!=(const WStringRef& lhs, const WString& rhs) noexcept { return !lhs.equals(rhs.ref()); }

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::WString>::operator()(const ca::str::WString& s) const noexcept {
    auto data = s.data();
    auto len  = s.length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

size_t hash<ca::str::WStringRef>::operator()(const ca::str::WStringRef& s) const noexcept {
    auto data = s.data();
    auto len  = s.length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
