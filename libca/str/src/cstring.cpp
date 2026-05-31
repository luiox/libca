//
// @brief CString / CStringRef / CStringBuilder 实现
// @author Canrad
// @date 2026/05/31
//

#include <libca/str/cstring.hpp>

#include <cstring>

namespace ca::str {

// ============================================================================
// CStringRef
// ============================================================================

CStringRef::CStringRef() noexcept : data_(nullptr), length_(0) {}
CStringRef::CStringRef(const char* data, usize length) noexcept : data_(data), length_(length) {}
CStringRef::CStringRef(const CString& str) noexcept : data_(str.data()), length_(str.length()) {}

usize CStringRef::length() const noexcept { return length_; }
bool CStringRef::isEmpty() const noexcept { return length_ == 0; }
const char* CStringRef::data() const noexcept { return data_; }

char CStringRef::at(usize index) const { return data_[index]; }

CStringRef CStringRef::slice(usize start, usize end) const {
    if (start >= end || start >= length_) return CStringRef();
    if (end > length_) end = length_;
    return CStringRef(data_ + start, end - start);
}

CString CStringRef::substr(usize start, usize count) const {
    auto ref = slice(start, start + count);
    return CString(ref.data(), ref.length());
}

int CStringRef::compare(const CStringRef& other) const noexcept {
    auto cmpLen = length_ < other.length_ ? length_ : other.length_;
    auto result = std::memcmp(data_, other.data_, cmpLen);
    if (result != 0) return result;
    if (length_ < other.length_) return -1;
    if (length_ > other.length_) return 1;
    return 0;
}

bool CStringRef::equals(const CStringRef& other) const noexcept {
    return length_ == other.length_ && std::memcmp(data_, other.data_, length_) == 0;
}

bool CStringRef::operator==(const CStringRef& other) const noexcept { return equals(other); }
bool CStringRef::operator!=(const CStringRef& other) const noexcept { return !equals(other); }


// ============================================================================
// CString
// ============================================================================

void CString::init(const char* src, usize len) {
    data_ = new char[len + 1];
    std::memcpy(data_, src, len);
    data_[len] = '\0';
    length_ = len;
}

CString::CString() noexcept : data_(new char[1]{0}), length_(0) {}

CString::CString(const char* data, usize length) : data_(nullptr), length_(0) {
    if (data == nullptr || length == 0) {
        data_ = new char[1]{0}; length_ = 0; return;
    }
    init(data, length);
}

CString::CString(const char* cstr) : data_(nullptr), length_(0) {
    if (cstr == nullptr) { data_ = new char[1]{0}; return; }
    init(cstr, std::strlen(cstr));
}

CString::CString(const CString& other) : data_(new char[other.length_ + 1]), length_(other.length_) {
    std::memcpy(data_, other.data_, length_ + 1);
}

CString::CString(CString&& other) noexcept : data_(other.data_), length_(other.length_) {
    other.data_ = nullptr; other.length_ = 0;
}

CString::~CString() { delete[] data_; }

CString& CString::operator=(const CString& other) {
    if (this != &other) {
        delete[] data_;
        length_ = other.length_;
        data_ = new char[length_ + 1];
        std::memcpy(data_, other.data_, length_ + 1);
    }
    return *this;
}

CString& CString::operator=(CString&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_; length_ = other.length_;
        other.data_ = nullptr; other.length_ = 0;
    }
    return *this;
}

CString CString::fromCStr(const char* cstr) { return CString(cstr); }

usize CString::length() const noexcept { return length_; }
bool CString::isEmpty() const noexcept { return length_ == 0; }
const char* CString::data() const noexcept { return data_; }
const char* CString::cStr() const noexcept { return data_; }
char CString::at(usize index) const { return data_[index]; }

CStringRef CString::ref() const noexcept { return CStringRef(data_, length_); }
CStringRef CString::slice(usize start, usize end) const { return ref().slice(start, end); }
CString CString::substr(usize start, usize count) const { return ref().substr(start, count); }

int CString::compare(const CStringRef& other) const noexcept { return ref().compare(other); }
int CString::compare(const CString& other) const noexcept { return ref().compare(other.ref()); }
bool CString::equals(const CStringRef& other) const noexcept { return ref().equals(other); }
bool CString::operator==(const CString& other) const noexcept { return ref().equals(other.ref()); }
bool CString::operator==(const CStringRef& other) const noexcept { return ref().equals(other); }
bool CString::operator!=(const CString& other) const noexcept { return !ref().equals(other.ref()); }
bool CString::operator!=(const CStringRef& other) const noexcept { return !ref().equals(other); }


// ============================================================================
// CStringBuilder
// ============================================================================

CStringBuilder::CStringBuilder() noexcept
    : buffer_(new char[kDefaultCapacity]), length_(0), capacity_(kDefaultCapacity) {}

CStringBuilder::CStringBuilder(const CStringBuilder& other)
    : buffer_(new char[other.capacity_]), length_(other.length_), capacity_(other.capacity_) {
    if (length_ > 0) std::memcpy(buffer_, other.buffer_, length_);
}

CStringBuilder::CStringBuilder(CStringBuilder&& other) noexcept
    : buffer_(other.buffer_), length_(other.length_), capacity_(other.capacity_) {
    other.buffer_ = nullptr; other.length_ = 0; other.capacity_ = 0;
}

CStringBuilder::~CStringBuilder() { delete[] buffer_; }

CStringBuilder& CStringBuilder::operator=(const CStringBuilder& other) {
    if (this != &other) {
        auto* newBuf = new char[other.capacity_];
        if (other.length_ > 0) std::memcpy(newBuf, other.buffer_, other.length_);
        delete[] buffer_;
        buffer_ = newBuf;
        length_ = other.length_;
        capacity_ = other.capacity_;
    }
    return *this;
}

CStringBuilder& CStringBuilder::operator=(CStringBuilder&& other) noexcept {
    if (this != &other) {
        delete[] buffer_;
        buffer_ = other.buffer_; length_ = other.length_; capacity_ = other.capacity_;
        other.buffer_ = nullptr; other.length_ = 0; other.capacity_ = 0;
    }
    return *this;
}

void CStringBuilder::grow(usize minCapacity) {
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity) newCap = minCapacity;
    if (newCap < kDefaultCapacity) newCap = kDefaultCapacity;
    auto newBuf = new char[newCap];
    if (length_ > 0) std::memcpy(newBuf, buffer_, length_);
    delete[] buffer_;
    buffer_ = newBuf; capacity_ = newCap;
}

CStringBuilder& CStringBuilder::append(const CStringRef& str) { return append(str.data(), str.length()); }
CStringBuilder& CStringBuilder::append(const CString& str) { return append(str.data(), str.length()); }

CStringBuilder& CStringBuilder::append(const char* cstr) {
    if (cstr == nullptr) return *this;
    return append(cstr, std::strlen(cstr));
}

CStringBuilder& CStringBuilder::append(const char* data, usize len) {
    if (len == 0) return *this;
    auto needed = length_ + len;
    if (needed > capacity_) grow(needed);
    std::memcpy(buffer_ + length_, data, len);
    length_ = needed;
    return *this;
}

CStringBuilder& CStringBuilder::append(char ch) { return append(&ch, 1); }

void CStringBuilder::reserve(usize cap) {
    if (cap > capacity_) {
        auto newBuf = new char[cap];
        if (length_ > 0) std::memcpy(newBuf, buffer_, length_);
        delete[] buffer_;
        buffer_ = newBuf; capacity_ = cap;
    }
}

usize CStringBuilder::capacity() const noexcept { return capacity_; }
usize CStringBuilder::length() const noexcept { return length_; }
bool CStringBuilder::isEmpty() const noexcept { return length_ == 0; }
void CStringBuilder::clear() noexcept { length_ = 0; }
CString CStringBuilder::build() const { return CString(buffer_, length_); }


// ============================================================================
// 非成员比较运算符
// ============================================================================

bool operator==(const CStringRef& lhs, const CString& rhs) noexcept { return lhs.equals(rhs.ref()); }
bool operator!=(const CStringRef& lhs, const CString& rhs) noexcept { return !lhs.equals(rhs.ref()); }

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::CString>::operator()(const ca::str::CString& s) const noexcept {
    auto data = s.data();
    auto len  = s.length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(static_cast<unsigned char>(data[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

size_t hash<ca::str::CStringRef>::operator()(const ca::str::CStringRef& s) const noexcept {
    auto data = s.data();
    auto len  = s.length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(static_cast<unsigned char>(data[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
