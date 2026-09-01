//
// @brief CString / CStringRef / CStringBuilder 实现
// @author Canrad
// @date 2026/05/31
//

#include "cstring.hpp"

#include <cctype>
#include <cstring>

namespace ca::str {

// ============================================================================
// CStringRef
// ============================================================================

CStringRef::CStringRef() noexcept
    : data_(nullptr)
    , length_(0)
{}
CStringRef::CStringRef(const char* data, usize length) noexcept
    : data_(data)
    , length_(length)
{}
CStringRef::CStringRef(const CString& str) noexcept
    : data_(str.data())
    , length_(str.length())
{}

usize CStringRef::length() const noexcept
{
    return length_;
}
bool CStringRef::is_empty() const noexcept
{
    return length_ == 0;
}
const char* CStringRef::data() const noexcept
{
    return data_;
}

char CStringRef::at(usize index) const
{
    return data_[index];
}

CStringRef CStringRef::slice(usize start, usize end) const
{
    if (start >= end || start >= length_)
        return CStringRef();
    if (end > length_)
        end = length_;
    return CStringRef(data_ + start, end - start);
}

CString CStringRef::substr(usize start, usize count) const
{
    auto ref = slice(start, start + count);
    return CString(ref.data(), ref.length());
}

int CStringRef::compare(const CStringRef& other) const noexcept
{
    auto cmpLen = length_ < other.length_ ? length_ : other.length_;
    auto result = std::memcmp(data_, other.data_, cmpLen);
    if (result != 0)
        return result;
    if (length_ < other.length_)
        return -1;
    if (length_ > other.length_)
        return 1;
    return 0;
}

bool CStringRef::equals(const CStringRef& other) const noexcept
{
    return length_ == other.length_ && std::memcmp(data_, other.data_, length_) == 0;
}

bool CStringRef::operator==(const CStringRef& other) const noexcept
{
    return equals(other);
}
bool CStringRef::operator!=(const CStringRef& other) const noexcept
{
    return !equals(other);
}


// ============================================================================
// CString
// ============================================================================

void CString::init(const char* src, usize len)
{
    data_ = new char[len + 1];
    std::memcpy(data_, src, len);
    data_[len] = '\0';
    length_    = len;
}

CString::CString() noexcept
    : data_(new char[1]{0})
    , length_(0)
{}

CString::CString(const char* data, usize length)
    : data_(nullptr)
    , length_(0)
{
    if (data == nullptr || length == 0) {
        data_   = new char[1]{0};
        length_ = 0;
        return;
    }
    init(data, length);
}

CString::CString(const char* cstr)
    : data_(nullptr)
    , length_(0)
{
    if (cstr == nullptr) {
        data_ = new char[1]{0};
        return;
    }
    init(cstr, std::strlen(cstr));
}

CString::CString(CString&& other) noexcept
    : data_(other.data_)
    , length_(other.length_)
{
    other.data_   = nullptr;
    other.length_ = 0;
}

CString::~CString()
{
    delete[] data_;
}

CString& CString::operator=(CString&& other) noexcept
{
    if (this != &other) {
        delete[] data_;
        data_         = other.data_;
        length_       = other.length_;
        other.data_   = nullptr;
        other.length_ = 0;
    }
    return *this;
}

CString CString::clone() const
{
    CString c;
    delete[] c.data_;
    c.data_ = new char[length_ + 1];
    std::memcpy(c.data_, data_, length_ + 1);
    c.length_ = length_;
    return c;
}

CString CString::from_cstr(const char* cstr)
{
    return CString(cstr);
}

usize CString::length() const noexcept
{
    return length_;
}
bool CString::is_empty() const noexcept
{
    return length_ == 0;
}
const char* CString::data() const noexcept
{
    return data_;
}
const char* CString::c_str() const noexcept
{
    return data_;
}
char CString::at(usize index) const
{
    return data_[index];
}

CStringRef CString::ref() const noexcept
{
    return CStringRef(data_, length_);
}
CStringRef CString::slice(usize start, usize end) const
{
    return ref().slice(start, end);
}
CString CString::substr(usize start, usize count) const
{
    return ref().substr(start, count);
}

int CString::compare(const CStringRef& other) const noexcept
{
    return ref().compare(other);
}
int CString::compare(const CString& other) const noexcept
{
    return ref().compare(other.ref());
}
bool CString::equals(const CStringRef& other) const noexcept
{
    return ref().equals(other);
}
bool CString::operator==(const CString& other) const noexcept
{
    return ref().equals(other.ref());
}
bool CString::operator==(const CStringRef& other) const noexcept
{
    return ref().equals(other);
}
bool CString::operator!=(const CString& other) const noexcept
{
    return !ref().equals(other.ref());
}
bool CString::operator!=(const CStringRef& other) const noexcept
{
    return !ref().equals(other);
}


// ============================================================================
// CStringRef — 新增操作
// ============================================================================

bool CStringRef::starts_with(const CStringRef& prefix) const noexcept
{
    if (prefix.length_ > length_)
        return false;
    return std::memcmp(data_, prefix.data_, prefix.length_) == 0;
}

bool CStringRef::ends_with(const CStringRef& suffix) const noexcept
{
    if (suffix.length_ > length_)
        return false;
    return std::memcmp(data_ + length_ - suffix.length_, suffix.data_, suffix.length_) == 0;
}

CStringRef CStringRef::trim_start() const noexcept
{
    usize i = 0;
    while (i < length_ && std::isspace(static_cast<unsigned char>(data_[i])))
        ++i;
    return slice(i, length_);
}

CStringRef CStringRef::trim_end() const noexcept
{
    usize i = length_;
    while (i > 0 && std::isspace(static_cast<unsigned char>(data_[i - 1])))
        --i;
    return slice(0, i);
}

CStringRef CStringRef::trim() const noexcept
{
    auto r = trim_start();
    return r.trim_end();
}

std::vector<CStringRef> CStringRef::split(const CStringRef& delimiter) const
{
    std::vector<CStringRef> result;
    if (is_empty())
        return result;
    if (delimiter.is_empty()) {
        result.push_back(*this);
        return result;
    }

    usize start = 0;
    while (true) {
        usize found = length_;
        for (usize i = start; i + delimiter.length_ <= length_; ++i) {
            if (std::memcmp(data_ + i, delimiter.data_, delimiter.length_) == 0) {
                found = i;
                break;
            }
        }
        result.push_back(CStringRef(data_ + start, found - start));
        if (found == length_)
            break;
        start = found + delimiter.length_;
    }
    return result;
}

CString CStringRef::to_lower() const
{
    CStringBuilder b;
    for (usize i = 0; i < length_; ++i)
        b.append(static_cast<char>(std::tolower(static_cast<unsigned char>(data_[i]))));
    return b.build();
}

CString CStringRef::to_upper() const
{
    CStringBuilder b;
    for (usize i = 0; i < length_; ++i)
        b.append(static_cast<char>(std::toupper(static_cast<unsigned char>(data_[i]))));
    return b.build();
}

CString CStringRef::replace_all(const CStringRef& from, const CStringRef& to) const
{
    if (from.is_empty())
        return CString(data_, length_);
    auto           parts = split(from);
    CStringBuilder b;
    for (usize i = 0; i < parts.size(); ++i) {
        if (i > 0)
            b.append(to);
        b.append(parts[i]);
    }
    return b.build();
}


// ============================================================================
// CString — 新增操作（委托给 ref）
// ============================================================================

bool CString::starts_with(const CStringRef& prefix) const noexcept
{
    return ref().starts_with(prefix);
}
bool CString::ends_with(const CStringRef& suffix) const noexcept
{
    return ref().ends_with(suffix);
}
CStringRef CString::trim() const noexcept
{
    return ref().trim();
}
CStringRef CString::trim_start() const noexcept
{
    return ref().trim_start();
}
CStringRef CString::trim_end() const noexcept
{
    return ref().trim_end();
}
std::vector<CStringRef> CString::split(const CStringRef& d) const
{
    return ref().split(d);
}
CString CString::to_lower() const
{
    return ref().to_lower();
}
CString CString::to_upper() const
{
    return ref().to_upper();
}
CString CString::replace_all(const CStringRef& from, const CStringRef& to) const
{
    return ref().replace_all(from, to);
}


// ============================================================================
// 非成员比较运算符 + 自由函数
// ============================================================================

bool operator==(const CStringRef& lhs, const CString& rhs) noexcept
{
    return lhs.equals(rhs.ref());
}

bool operator!=(const CStringRef& lhs, const CString& rhs) noexcept
{
    return !lhs.equals(rhs.ref());
}

std::vector<CStringRef> split(const CStringRef& str, const CStringRef& delimiter)
{
    return str.split(delimiter);
}

CString join(const std::vector<CStringRef>& parts, const CStringRef& separator)
{
    CStringBuilder b;
    for (usize i = 0; i < parts.size(); ++i) {
        if (i > 0)
            b.append(separator);
        b.append(parts[i]);
    }
    return b.build();
}


// ============================================================================
// CStringBuilder
// ============================================================================

CStringBuilder::CStringBuilder() noexcept
    : buffer_(new char[kDefaultCapacity])
    , length_(0)
    , capacity_(kDefaultCapacity)
{}

CStringBuilder::CStringBuilder(CStringBuilder&& other) noexcept
    : buffer_(other.buffer_)
    , length_(other.length_)
    , capacity_(other.capacity_)
{
    other.buffer_   = nullptr;
    other.length_   = 0;
    other.capacity_ = 0;
}

CStringBuilder::~CStringBuilder()
{
    delete[] buffer_;
}

CStringBuilder& CStringBuilder::operator=(CStringBuilder&& other) noexcept
{
    if (this != &other) {
        delete[] buffer_;
        buffer_         = other.buffer_;
        length_         = other.length_;
        capacity_       = other.capacity_;
        other.buffer_   = nullptr;
        other.length_   = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void CStringBuilder::grow(usize minCapacity)
{
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity)
        newCap = minCapacity;
    if (newCap < kDefaultCapacity)
        newCap = kDefaultCapacity;
    auto newBuf = new char[newCap];
    if (length_ > 0)
        std::memcpy(newBuf, buffer_, length_);
    delete[] buffer_;
    buffer_   = newBuf;
    capacity_ = newCap;
}

CStringBuilder& CStringBuilder::append(const CStringRef& str)
{
    return append(str.data(), str.length());
}
CStringBuilder& CStringBuilder::append(const CString& str)
{
    return append(str.data(), str.length());
}

CStringBuilder& CStringBuilder::append(const char* cstr)
{
    if (cstr == nullptr)
        return *this;
    return append(cstr, std::strlen(cstr));
}

CStringBuilder& CStringBuilder::append(const char* data, usize len)
{
    if (len == 0)
        return *this;
    auto needed = length_ + len;
    if (needed > capacity_)
        grow(needed);
    std::memcpy(buffer_ + length_, data, len);
    length_ = needed;
    return *this;
}

CStringBuilder& CStringBuilder::append(char ch)
{
    return append(&ch, 1);
}

void CStringBuilder::reserve(usize cap)
{
    if (cap > capacity_) {
        auto newBuf = new char[cap];
        if (length_ > 0)
            std::memcpy(newBuf, buffer_, length_);
        delete[] buffer_;
        buffer_   = newBuf;
        capacity_ = cap;
    }
}

usize CStringBuilder::capacity() const noexcept
{
    return capacity_;
}
usize CStringBuilder::length() const noexcept
{
    return length_;
}
bool CStringBuilder::is_empty() const noexcept
{
    return length_ == 0;
}
void CStringBuilder::clear() noexcept
{
    length_ = 0;
}
CString CStringBuilder::build() const
{
    return CString(buffer_, length_);
}

}   // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::CString>::operator()(const ca::str::CString& s) const noexcept
{
    auto   data = s.data();
    auto   len  = s.length();
    size_t h    = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(static_cast<unsigned char>(data[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

size_t hash<ca::str::CStringRef>::operator()(const ca::str::CStringRef& s) const noexcept
{
    auto   data = s.data();
    auto   len  = s.length();
    size_t h    = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(static_cast<unsigned char>(data[i]));
        h *= 1099511628211ULL;
    }
    return h;
}

}   // namespace std
