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
#include <unordered_map>

namespace ca::str {


// ============================================================================
// Utf8StringRef
// ============================================================================

Utf8StringRef::Utf8StringRef() noexcept
    : data_(nullptr)
    , byte_length_(0)
    , length_(0) {}

Utf8StringRef::Utf8StringRef(const u8* data, usize byte_length, usize length) noexcept
    : data_(data)
    , byte_length_(byte_length)
    , length_(length) {}

Utf8StringRef::Utf8StringRef(const Utf8String& str) noexcept
    : data_(str.data())
    , byte_length_(str.byte_length())
    , length_(str.length()) {}

Utf8StringRef Utf8StringRef::from_cstr(const char* cstr) noexcept {
    if (cstr == nullptr) return Utf8StringRef();
    auto len  = std::strlen(cstr);
    auto data = reinterpret_cast<const u8*>(cstr);
    return Utf8StringRef(data, len, utf8_count_code_points(data, len));
}

Utf8StringRef Utf8StringRef::from_data(const u8* data, usize byte_len, usize cp_len) {
    if (data == nullptr || byte_len == 0)
        return Utf8StringRef();
    usize cp = (cp_len == usize(-1))
        ? utf8_count_code_points(data, byte_len)
        : cp_len;
    return Utf8StringRef(data, byte_len, cp);
}

Utf8StringRef Utf8StringRef::from_string_view(std::string_view sv) noexcept {
    if (sv.empty())
        return Utf8StringRef();
    auto data = reinterpret_cast<const u8*>(sv.data());
    auto byte_len = static_cast<usize>(sv.size());
    return Utf8StringRef(data, byte_len, utf8_count_code_points(data, byte_len));
}

usize Utf8StringRef::length() const noexcept {
    return length_;
}

usize Utf8StringRef::byte_length() const noexcept {
    return byte_length_;
}

bool Utf8StringRef::is_empty() const noexcept {
    return byte_length_ == 0;
}

const u8* Utf8StringRef::data() const noexcept {
    return data_;
}

Utf8StringRef::operator std::string_view() const noexcept {
    return std::string_view(reinterpret_cast<const char*>(data_),
                            static_cast<std::string_view::size_type>(byte_length_));
}

std::string Utf8StringRef::to_std_string() const {
    if (data_ == nullptr || byte_length_ == 0)
        return std::string();
    return std::string(reinterpret_cast<const char*>(data_), byte_length_);
}

u8 Utf8StringRef::byte_at(usize index) const {
    return data_[index];
}

u32 Utf8StringRef::code_point_at(usize index) const {
    usize pos   = 0;
    usize cp_idx = 0;
    while (cp_idx < index && pos < byte_length_) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++cp_idx;
    }
    if (pos >= byte_length_)
        return 0;
    return utf8_decode_code_point(data_ + pos);
}

Utf8StringRef Utf8StringRef::slice(usize byte_start, usize byte_end) const {
    if (byte_start >= byte_end || byte_start >= byte_length_)
        return Utf8StringRef();

    if (byte_end > byte_length_)
        byte_end = byte_length_;

    // 从 byte_start 开始扫描，计算区间内的码点个数
    usize pos    = byte_start;
    usize cp_cnt = 0;
    while (pos < byte_end) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++cp_cnt;
    }

    return Utf8StringRef(data_ + byte_start, byte_end - byte_start, cp_cnt);
}

Utf8StringRef Utf8StringRef::slice_by_cp(usize cp_start, usize cp_count) const {
    if (cp_start >= length_ || cp_count == 0)
        return Utf8StringRef();

    // 扫描到 cp_start
    usize pos = 0;
    for (usize i = 0; i < cp_start; ++i) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }

    usize start_pos = pos;

    // 扫描 cp_count 个码点
    usize actual_count = 0;
    for (usize i = 0; i < cp_count && pos < byte_length_; ++i) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++actual_count;
    }

    return Utf8StringRef(data_ + start_pos, pos - start_pos, actual_count);
}

Utf8String Utf8StringRef::substr(usize cp_start, usize cp_count) const {
    auto ref = slice_by_cp(cp_start, cp_count);
    return Utf8String(ref.data(), ref.byte_length());
}

int Utf8StringRef::compare(const Utf8StringRef& other) const noexcept {
    auto cmp_len = byte_length_ < other.byte_length_
                      ? byte_length_
                      : other.byte_length_;

    auto result = std::memcmp(data_, other.data_, cmp_len);
    if (result != 0)
        return result;

    // 前缀相同，较短的字符串小于较长的
    if (byte_length_ < other.byte_length_)
        return -1;
    if (byte_length_ > other.byte_length_)
        return 1;
    return 0;
}

int Utf8StringRef::compare(const char* cstr) const noexcept {
    auto other_len = cstr ? static_cast<usize>(std::strlen(cstr)) : 0;
    auto other_data = reinterpret_cast<const u8*>(cstr);
    auto cmp_len = byte_length_ < other_len ? byte_length_ : other_len;

    if (cmp_len > 0) {
        auto result = std::memcmp(data_, other_data, cmp_len);
        if (result != 0)
            return result;
    }

    if (byte_length_ < other_len)
        return -1;
    if (byte_length_ > other_len)
        return 1;
    return 0;
}

bool Utf8StringRef::equals(const Utf8StringRef& other) const noexcept {
    return byte_length_ == other.byte_length_ &&
           std::memcmp(data_, other.data_, byte_length_) == 0;
}

bool Utf8StringRef::equals(const char* cstr) const noexcept {
    auto other_len = cstr ? static_cast<usize>(std::strlen(cstr)) : 0;
    return byte_length_ == other_len &&
           (byte_length_ == 0 ||
            std::memcmp(data_, cstr, byte_length_) == 0);
}

bool Utf8StringRef::operator==(const Utf8StringRef& other) const noexcept {
    return equals(other);
}

bool Utf8StringRef::operator==(const char* cstr) const noexcept {
    return equals(cstr);
}

bool Utf8StringRef::operator!=(const Utf8StringRef& other) const noexcept {
    return !equals(other);
}

bool Utf8StringRef::operator!=(const char* cstr) const noexcept {
    return !equals(cstr);
}

// ============================================================================
// Utf8String
// ============================================================================

void Utf8String::init(const u8* src, usize byte_len) {
    if (src == nullptr || byte_len == 0) {
        data_        = new u8[1]{0};
        byte_length_ = 0;
        length_      = 0;
        return;
    }

    // 校验 UTF-8 合法性并统计码点个数
    usize invalid_pos = 0;
    auto  cp_count    = utf8_count_code_points(src, byte_len, &invalid_pos);
    if (cp_count == 0) {
        throw std::runtime_error(
            "Utf8String: invalid UTF-8 sequence at byte position "
            + std::to_string(invalid_pos));
    }

    data_ = new u8[byte_len + 1];
    std::memcpy(data_, src, byte_len);
    data_[byte_len] = '\0';

    byte_length_ = byte_len;
    length_      = cp_count;
}

Utf8String::Utf8String() noexcept
    : data_(new u8[1]{0})
    , byte_length_(0)
    , length_(0) {}

Utf8String::Utf8String(const u8* data, usize byte_length)
    : data_(nullptr)
    , byte_length_(0)
    , length_(0) {
    init(data, byte_length);
}

Utf8String::Utf8String(const char* cstr)
    : data_(nullptr)
    , byte_length_(0)
    , length_(0) {
    if (cstr == nullptr) {
        data_ = new u8[1]{0};
        return;
    }
    auto len = std::strlen(cstr);
    init(reinterpret_cast<const u8*>(cstr), len);
}

Utf8String::Utf8String(Utf8String&& other) noexcept
    : data_(other.data_)
    , byte_length_(other.byte_length_)
    , length_(other.length_) {
    other.data_        = nullptr;
    other.byte_length_ = 0;
    other.length_      = 0;
}

Utf8String::~Utf8String() {
    delete[] data_;
    data_ = nullptr;
}

Utf8String& Utf8String::operator=(Utf8String&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_        = other.data_;
        byte_length_ = other.byte_length_;
        length_      = other.length_;
        other.data_        = nullptr;
        other.byte_length_ = 0;
        other.length_      = 0;
    }
    return *this;
}

Utf8String Utf8String::clone() const {
    Utf8String s;
    delete[] s.data_;
    s.data_ = new u8[byte_length_ + 1];
    std::memcpy(s.data_, data_, byte_length_ + 1);
    s.byte_length_ = byte_length_;
    s.length_      = length_;
    return s;
}

Utf8String Utf8String::from_code_point(u32 cp) {
    u8 buf[4];
    auto len = utf8_encode_code_point(cp, buf);
    if (len == 0) {
        throw std::runtime_error(
            "Utf8String::from_code_point: invalid code point U+"
            + std::to_string(cp));
    }
    return Utf8String(buf, len);
}

Utf8String Utf8String::from_cstr(const char* cstr) noexcept {
    try {
        return Utf8String(cstr);
    } catch (...) {
        return Utf8String();
    }
}

Utf8String Utf8String::from_data(const u8* data, usize byte_len, usize cp_len) {
    if (data == nullptr || byte_len == 0)
        return Utf8String();

    if (cp_len == usize(-1)) {
        // 未传入码点数，走标准构造（校验 + 计数）
        return Utf8String(data, byte_len);
    }

    // 传入了码点数：仅校验合法性，复用 cp_len 避免重复计数
    if (!utf8_is_valid(data, byte_len)) {
        throw std::runtime_error(
            "Utf8String::from_data: invalid UTF-8 sequence");
    }

    Utf8String s;
    delete[] s.data_;
    s.data_ = new u8[byte_len + 1];
    std::memcpy(s.data_, data, byte_len);
    s.data_[byte_len] = '\0';
    s.byte_length_    = byte_len;
    s.length_         = cp_len;
    return s;
}


// ============================================================================
// Utf8StringBuilder
// ============================================================================

Utf8StringBuilder::Utf8StringBuilder() noexcept
    : buffer_(new u8[DEFAULT_CAPACITY]), byte_length_(0), capacity_(DEFAULT_CAPACITY) {}

Utf8StringBuilder::Utf8StringBuilder(Utf8StringBuilder&& other) noexcept
    : buffer_(other.buffer_), byte_length_(other.byte_length_), capacity_(other.capacity_) {
    other.buffer_ = nullptr; other.byte_length_ = 0; other.capacity_ = 0;
}

Utf8StringBuilder::~Utf8StringBuilder() { delete[] buffer_; }

Utf8StringBuilder& Utf8StringBuilder::operator=(Utf8StringBuilder&& other) noexcept {
    if (this != &other) {
        delete[] buffer_;
        buffer_ = other.buffer_; byte_length_ = other.byte_length_; capacity_ = other.capacity_;
        other.buffer_ = nullptr; other.byte_length_ = 0; other.capacity_ = 0;
    }
    return *this;
}

void Utf8StringBuilder::grow(usize min_capacity) {
    usize new_cap = capacity_ * 2;
    if (new_cap < min_capacity) new_cap = min_capacity;
    if (new_cap < DEFAULT_CAPACITY) new_cap = DEFAULT_CAPACITY;
    auto new_buf = new u8[new_cap];
    if (byte_length_ > 0) std::memcpy(new_buf, buffer_, byte_length_);
    delete[] buffer_;
    buffer_ = new_buf; capacity_ = new_cap;
}

Utf8StringBuilder& Utf8StringBuilder::append(const Utf8StringRef& str) {
    return append(str.data(), str.byte_length());
}

Utf8StringBuilder& Utf8StringBuilder::append(const Utf8String& str) {
    return append(str.data(), str.byte_length());
}

Utf8StringBuilder& Utf8StringBuilder::append(const char* cstr) {
    if (cstr == nullptr) return *this;
    return append(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringBuilder& Utf8StringBuilder::append(const u8* data, usize byte_len) {
    if (byte_len == 0) return *this;
    auto needed = byte_length_ + byte_len;
    if (needed > capacity_) grow(needed);
    std::memcpy(buffer_ + byte_length_, data, byte_len);
    byte_length_ = needed;
    return *this;
}

bool Utf8StringBuilder::append_code_point(u32 cp) {
    u8 buf[4];
    auto len = utf8_encode_code_point(cp, buf);
    if (len == 0) return false;
    append(buf, len);
    return true;
}

void Utf8StringBuilder::reserve(usize byte_capacity) {
    if (byte_capacity > capacity_) {
        auto new_buf = new u8[byte_capacity];
        if (byte_length_ > 0) std::memcpy(new_buf, buffer_, byte_length_);
        delete[] buffer_;
        buffer_ = new_buf; capacity_ = byte_capacity;
    }
}

usize Utf8StringBuilder::capacity() const noexcept { return capacity_; }
usize Utf8StringBuilder::byte_length() const noexcept { return byte_length_; }
bool Utf8StringBuilder::is_empty() const noexcept { return byte_length_ == 0; }
void Utf8StringBuilder::clear() noexcept { byte_length_ = 0; }

Utf8String Utf8StringBuilder::build() const {
    return Utf8String(buffer_, byte_length_);
}

Utf8String Utf8StringBuilder::build_or_empty() const noexcept {
    if (!utf8_is_valid(buffer_, byte_length_))
        return Utf8String();
    return Utf8String(buffer_, byte_length_);
}


usize Utf8String::length() const noexcept {
    return length_;
}

usize Utf8String::byte_length() const noexcept {
    return byte_length_;
}

bool Utf8String::is_empty() const noexcept {
    return byte_length_ == 0;
}

const u8* Utf8String::data() const noexcept {
    return data_;
}

const char* Utf8String::c_str() const noexcept {
    return reinterpret_cast<const char*>(data_);
}

Utf8String::operator std::string_view() const noexcept {
    return std::string_view(reinterpret_cast<const char*>(data_),
                            static_cast<std::string_view::size_type>(byte_length_));
}

std::string Utf8String::to_std_string() const {
    return std::string(reinterpret_cast<const char*>(data_), byte_length_);
}

u8 Utf8String::byte_at(usize index) const {
    return data_[index];
}

u32 Utf8String::code_point_at(usize index) const {
    usize pos   = 0;
    usize cp_idx = 0;
    while (cp_idx < index && pos < byte_length_) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++cp_idx;
    }
    if (pos >= byte_length_)
        return 0;
    return utf8_decode_code_point(data_ + pos);
}

Utf8StringRef Utf8String::ref() const noexcept {
    return Utf8StringRef(data_, byte_length_, length_);
}

Utf8StringRef Utf8String::slice(usize byte_start, usize byte_end) const {
    return ref().slice(byte_start, byte_end);
}

Utf8StringRef Utf8String::slice_by_cp(usize cp_start, usize cp_count) const {
    return ref().slice_by_cp(cp_start, cp_count);
}

Utf8String Utf8String::substr(usize cp_start, usize cp_count) const {
    return ref().substr(cp_start, cp_count);
}

int Utf8String::compare(const Utf8StringRef& other) const noexcept {
    return ref().compare(other);
}

int Utf8String::compare(const Utf8String& other) const noexcept {
    return ref().compare(other.ref());
}

int Utf8String::compare(const char* cstr) const noexcept {
    return ref().compare(cstr);
}

bool Utf8String::equals(const Utf8StringRef& other) const noexcept {
    return ref().equals(other);
}

bool Utf8String::equals(const char* cstr) const noexcept {
    return ref().equals(cstr);
}

bool Utf8String::operator==(const Utf8String& other) const noexcept {
    return ref().equals(other.ref());
}

bool Utf8String::operator==(const Utf8StringRef& other) const noexcept {
    return ref().equals(other);
}

bool Utf8String::operator==(const char* cstr) const noexcept {
    return ref().equals(cstr);
}

bool Utf8String::operator!=(const Utf8String& other) const noexcept {
    return !ref().equals(other.ref());
}

bool Utf8String::operator!=(const Utf8StringRef& other) const noexcept {
    return !ref().equals(other);
}

bool Utf8String::operator!=(const char* cstr) const noexcept {
    return !ref().equals(cstr);
}

// ============================================================================
// Utf8StringRef — 新增操作
// ============================================================================

bool Utf8StringRef::starts_with(const Utf8StringRef& prefix) const noexcept {
    if (prefix.byte_length_ > byte_length_) return false;
    return std::memcmp(data_, prefix.data_, prefix.byte_length_) == 0;
}

bool Utf8StringRef::ends_with(const Utf8StringRef& suffix) const noexcept {
    if (suffix.byte_length_ > byte_length_) return false;
    return std::memcmp(data_ + byte_length_ - suffix.byte_length_,
                       suffix.data_, suffix.byte_length_) == 0;
}

Utf8StringRef Utf8StringRef::trim_start() const noexcept {
    usize pos = 0;
    while (pos < byte_length_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        if (!ch.isSpace()) break;
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }
    return slice(pos, byte_length_);
}

Utf8StringRef Utf8StringRef::trim_end() const noexcept {
    usize pos = byte_length_;
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
    auto r = trim_start();
    return r.trim_end();
}

std::vector<Utf8StringRef> Utf8StringRef::split(const Utf8StringRef& delimiter) const {
    std::vector<Utf8StringRef> result;
    if (is_empty()) return result;
    if (delimiter.is_empty()) { result.push_back(*this); return result; }

    usize start = 0;
    while (start <= byte_length_) {
        usize remain = byte_length_ - start;
        usize found = usize(-1);
        // 查找分隔符
        for (usize i = start; i + delimiter.byte_length_ <= byte_length_; ) {
            if (std::memcmp(data_ + i, delimiter.data_, delimiter.byte_length_) == 0) {
                found = i;
                break;
            }
            i += utf8_code_point_bytes_safe(data_[i]);
        }
        if (found == usize(-1)) {
            // 未找到，剩余整个作为最后一段
            auto ref = slice(start, byte_length_);
            result.push_back(ref);
            break;
        }
        auto ref = slice(start, found);
        result.push_back(ref);
        start = found + delimiter.byte_length_;
    }
    return result;
}

Utf8String Utf8StringRef::to_lower() const {
    Utf8StringBuilder b;
    usize pos = 0;
    while (pos < byte_length_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        b.append_code_point(ch.toLower().codePoint());
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }
    return b.build();
}

Utf8String Utf8StringRef::to_upper() const {
    Utf8StringBuilder b;
    usize pos = 0;
    while (pos < byte_length_) {
        auto ch = Utf8Char::fromRaw(data_ + pos);
        b.append_code_point(ch.toUpper().codePoint());
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }
    return b.build();
}

Utf8String Utf8StringRef::replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const {
    if (from.is_empty()) return Utf8String(data_, byte_length_);
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

usize Utf8StringRef::index_of(const Utf8StringRef& needle) const noexcept {
    return index_of(needle, 0);
}

usize Utf8StringRef::index_of(const Utf8StringRef& needle, usize start_cp) const noexcept {
    if (needle.is_empty() || needle.byte_length_ > byte_length_)
        return npos;

    usize pos = 0;
    for (usize i = 0; i < start_cp && pos < byte_length_; ++i) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }

    usize cp_idx = start_cp;
    while (pos + needle.byte_length_ <= byte_length_) {
        if (std::memcmp(data_ + pos, needle.data_, needle.byte_length_) == 0)
            return cp_idx;
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++cp_idx;
    }
    return npos;
}

usize Utf8StringRef::index_of(u32 code_point) const noexcept {
    return index_of(code_point, 0);
}

usize Utf8StringRef::index_of(u32 code_point, usize start_cp) const noexcept {
    usize pos = 0;
    for (usize i = 0; i < start_cp && pos < byte_length_; ++i) {
        pos += utf8_code_point_bytes_safe(data_[pos]);
    }
    usize cp_idx = start_cp;
    while (pos < byte_length_) {
        if (utf8_decode_code_point(data_ + pos) == code_point)
            return cp_idx;
        pos += utf8_code_point_bytes_safe(data_[pos]);
        ++cp_idx;
    }
    return npos;
}

bool Utf8StringRef::contains(const Utf8StringRef& needle) const noexcept {
    return index_of(needle) != npos;
}


// ============================================================================
// Utf8StringRef — 迭代器
// ============================================================================

Utf8Iterator Utf8StringRef::begin() const noexcept {
    return Utf8Iterator(data_, data_ + byte_length_);
}

Utf8Iterator Utf8StringRef::end() const noexcept {
    return Utf8Iterator(data_ + byte_length_, data_ + byte_length_);
}


// ============================================================================
// Utf8String — 新增操作（委托给 ref）
// ============================================================================

bool Utf8String::starts_with(const Utf8StringRef& prefix) const noexcept { return ref().starts_with(prefix); }
bool Utf8String::ends_with(const Utf8StringRef& suffix) const noexcept   { return ref().ends_with(suffix); }
Utf8StringRef Utf8String::trim() const noexcept        { return ref().trim(); }
Utf8StringRef Utf8String::trim_start() const noexcept  { return ref().trim_start(); }
Utf8StringRef Utf8String::trim_end() const noexcept    { return ref().trim_end(); }
std::vector<Utf8StringRef> Utf8String::split(const Utf8StringRef& d) const { return ref().split(d); }
Utf8String Utf8String::to_lower() const    { return ref().to_lower(); }
Utf8String Utf8String::to_upper() const    { return ref().to_upper(); }
Utf8String Utf8String::replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const {
    return ref().replace_all(from, to);
}

usize Utf8String::index_of(const Utf8StringRef& needle) const noexcept {
    return ref().index_of(needle);
}

usize Utf8String::index_of(const Utf8StringRef& needle, usize start_cp) const noexcept {
    return ref().index_of(needle, start_cp);
}

usize Utf8String::index_of(u32 code_point) const noexcept {
    return ref().index_of(code_point);
}

usize Utf8String::index_of(u32 code_point, usize start_cp) const noexcept {
    return ref().index_of(code_point, start_cp);
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

// 排序统一落在非成员 Utf8StringRef 重载上；Utf8String 通过隐式视图构造复用这里。
bool operator<(const Utf8StringRef& lhs, const Utf8StringRef& rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

bool operator>(const Utf8StringRef& lhs, const Utf8StringRef& rhs) noexcept {
    return lhs.compare(rhs) > 0;
}

bool operator<=(const Utf8StringRef& lhs, const Utf8StringRef& rhs) noexcept {
    return lhs.compare(rhs) <= 0;
}

bool operator>=(const Utf8StringRef& lhs, const Utf8StringRef& rhs) noexcept {
    return lhs.compare(rhs) >= 0;
}

bool operator<(const Utf8StringRef& lhs, const char* rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

bool operator>(const Utf8StringRef& lhs, const char* rhs) noexcept {
    return lhs.compare(rhs) > 0;
}

bool operator<=(const Utf8StringRef& lhs, const char* rhs) noexcept {
    return lhs.compare(rhs) <= 0;
}

bool operator>=(const Utf8StringRef& lhs, const char* rhs) noexcept {
    return lhs.compare(rhs) >= 0;
}

bool operator==(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return rhs.equals(lhs);
}

bool operator!=(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return !rhs.equals(lhs);
}

bool operator<(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return rhs.compare(lhs) > 0;
}

bool operator>(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return rhs.compare(lhs) < 0;
}

bool operator<=(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return rhs.compare(lhs) >= 0;
}

bool operator>=(const char* lhs, const Utf8StringRef& rhs) noexcept {
    return rhs.compare(lhs) <= 0;
}

bool operator==(const char* lhs, const Utf8String& rhs) noexcept {
    return rhs.equals(lhs);
}

bool operator!=(const char* lhs, const Utf8String& rhs) noexcept {
    return !rhs.equals(lhs);
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
                 static_cast<std::streamsize>(s.byte_length()));
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

static CacheData& get_cache() {
    static CacheData cache;
    return cache;
}

ZUtf8StringRef ZUtf8StringRef::from_static(const char* cstr)
{
    if (!cstr) return ZUtf8StringRef(nullptr, 0, 0);

    auto& inst = get_cache();
    std::lock_guard<std::mutex> lock(inst.mutex);

    auto it = inst.map.find(cstr);
    if (it != inst.map.end()) {
        return it->second; // 命中缓存
    }

    // 未命中，计算并加入缓存
    usize len = std::strlen(cstr);
    usize cp = utf8_count_code_points(reinterpret_cast<const u8*>(cstr), len);
    ZUtf8StringRef ref(reinterpret_cast<const u8*>(cstr), len, cp);
    inst.map.emplace(cstr, ref);
    return ref;
}

ZUtf8StringRef ZUtf8StringRef::from_utf8_string(const Utf8String& s) {
    return ZUtf8StringRef(s.data(), s.byte_length(), s.length());
}

ZUtf8StringRef ZUtf8StringRef::from_std_string(const std::string& s)
{
    auto data = reinterpret_cast<const u8*>(s.data());
    auto byte_length = static_cast<usize>(s.size());
    auto cp_length = utf8_count_code_points(data, byte_length);
    return ZUtf8StringRef(data, byte_length, cp_length);
}

Utf8StringRef ZUtf8StringRef::ref() const noexcept {
    return Utf8StringRef(data_, byte_length_, cp_length_);
}

ZUtf8StringRef::operator Utf8StringRef() const noexcept {
    return ref();
}

int ZUtf8StringRef::compare(const Utf8StringRef& other) const noexcept {
    return ref().compare(other);
}

int ZUtf8StringRef::compare(const char* cstr) const noexcept {
    return ref().compare(cstr);
}

bool ZUtf8StringRef::equals(const Utf8StringRef& other) const noexcept {
    return ref().equals(other);
}

bool ZUtf8StringRef::equals(const char* cstr) const noexcept {
    return ref().equals(cstr);
}

bool ZUtf8StringRef::operator==(const Utf8StringRef& other) const noexcept {
    return equals(other);
}

bool ZUtf8StringRef::operator==(const char* cstr) const noexcept {
    return equals(cstr);
}

bool ZUtf8StringRef::operator!=(const Utf8StringRef& other) const noexcept {
    return !equals(other);
}

bool ZUtf8StringRef::operator!=(const char* cstr) const noexcept {
    return !equals(cstr);
}

bool operator==(const char* lhs, const ZUtf8StringRef& rhs) noexcept {
    return rhs.equals(lhs);
}

bool operator!=(const char* lhs, const ZUtf8StringRef& rhs) noexcept {
    return !rhs.equals(lhs);
}

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::Utf8String>::operator()(
    const ca::str::Utf8String& s) const noexcept {
    auto data = s.data();
    auto len  = s.byte_length();
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
    auto len  = s.byte_length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
