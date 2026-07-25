#include "utf16_string.hpp"

#include "char_util.hpp"
#include "conversion.hpp"

#include <cstring>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace ca::str {
namespace {

bool is_valid_code_point(ca::u32 cp) noexcept
{
    return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

bool encode_code_point(ca::u32 cp, Char16& high, Char16& low) noexcept
{
    if (!is_valid_code_point(cp))
        return false;
    if (cp < 0x10000) {
        high = Char16(static_cast<ca::u16>(cp));
        low = Char16();
        return true;
    }

    Utf16Char high_char;
    Utf16Char low_char;
    if (!Utf16Char::encode_pair(cp, high_char, low_char))
        return false;
    high = Char16(high_char.unit());
    low = Char16(low_char.unit());
    return true;
}

ca::u32 decode_pair(Char16 high, Char16 low) noexcept
{
    return Utf16Char::decode_pair(Utf16Char(high.unit()), Utf16Char(low.unit()));
}

bool matches_at(const Char16* data, ca::usize length, const Utf16StringRef& needle, ca::usize offset) noexcept
{
    if (needle.length() == 0)
        return offset <= length;
    if (data == nullptr || needle.data() == nullptr || offset > length || needle.length() > length - offset)
        return false;

    for (ca::usize i = 0; i < needle.length(); ++i) {
        if (data[offset + i] != needle.data()[i])
            return false;
    }
    return true;
}

Utf16String utf16_from_ascii(const char* text)
{
    return Utf16String::from_utf8_string(Utf8StringRef::from_cstr(text));
}

Utf16String utf16_from_number_text(const std::string& text)
{
    return Utf16String::from_utf8_string(
        Utf8StringRef::from_data(reinterpret_cast<const ca::u8*>(text.data()),
                                 static_cast<ca::usize>(text.size())));
}

template<typename T>
Utf16String value_of_with_stream(T value)
{
    std::ostringstream out;
    out << value;
    return utf16_from_number_text(out.str());
}

}  // namespace

Utf16StringRef::Utf16StringRef(const Utf16String& str) noexcept
    : data_(str.data()), length_(str.length()) {}

Utf16StringRef Utf16StringRef::from_data(const ca::u16* data, ca::usize length) noexcept
{
    return Utf16StringRef(reinterpret_cast<const Char16*>(data), length);
}

Utf16StringRef Utf16StringRef::from_std_u16string(const std::u16string& str) noexcept
{
    return from_data(reinterpret_cast<const ca::u16*>(str.data()), static_cast<ca::usize>(str.size()));
}

const ca::u16* Utf16StringRef::raw_data() const noexcept
{
    return reinterpret_cast<const ca::u16*>(data_);
}

Char16 Utf16StringRef::char_at(ca::usize index) const noexcept
{
    if (index >= length_ || data_ == nullptr)
        return Char16();
    return data_[index];
}

ca::u32 Utf16StringRef::code_point_at(ca::usize index) const noexcept
{
    if (index >= length_ || data_ == nullptr)
        return 0;

    const auto first = data_[index];
    if (first.is_lead_surrogate() && index + 1 < length_) {
        const auto second = data_[index + 1];
        if (second.is_trail_surrogate())
            return decode_pair(first, second);
    }
    return first.unit();
}

Utf16StringRef Utf16StringRef::slice(ca::usize begin, ca::usize end) const noexcept
{
    if (begin >= end || begin >= length_ || data_ == nullptr)
        return Utf16StringRef();
    if (end > length_)
        end = length_;
    return Utf16StringRef(data_ + begin, end - begin);
}

Utf16String Utf16StringRef::substring(ca::usize begin, ca::usize end) const
{
    auto view = slice(begin, end);
    return Utf16String(view.data(), view.length());
}

ca::usize Utf16StringRef::index_of(Char16 ch) const noexcept
{
    return index_of(ch, 0);
}

ca::usize Utf16StringRef::index_of(Char16 ch, ca::usize from) const noexcept
{
    if (data_ == nullptr || from >= length_)
        return npos;
    for (ca::usize i = from; i < length_; ++i) {
        if (data_[i] == ch)
            return i;
    }
    return npos;
}

ca::usize Utf16StringRef::index_of(const Utf16StringRef& needle) const noexcept
{
    return index_of(needle, 0);
}

ca::usize Utf16StringRef::index_of(const Utf16StringRef& needle, ca::usize from) const noexcept
{
    if (needle.is_empty())
        return from <= length_ ? from : npos;
    if (data_ == nullptr || needle.data_ == nullptr || from >= length_ || needle.length_ > length_)
        return npos;

    for (ca::usize i = from; i + needle.length_ <= length_; ++i) {
        bool match = true;
        for (ca::usize j = 0; j < needle.length_; ++j) {
            if (data_[i + j] != needle.data_[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return npos;
}

ca::usize Utf16StringRef::last_index_of(Char16 ch) const noexcept
{
    if (length_ == 0)
        return npos;
    return last_index_of(ch, length_ - 1);
}

ca::usize Utf16StringRef::last_index_of(Char16 ch, ca::usize from) const noexcept
{
    if (data_ == nullptr || length_ == 0)
        return npos;

    ca::usize i = from < length_ ? from : length_ - 1;
    for (;;) {
        if (data_[i] == ch)
            return i;
        if (i == 0)
            break;
        --i;
    }
    return npos;
}

ca::usize Utf16StringRef::last_index_of(const Utf16StringRef& needle) const noexcept
{
    return last_index_of(needle, length_);
}

ca::usize Utf16StringRef::last_index_of(const Utf16StringRef& needle, ca::usize from) const noexcept
{
    if (needle.is_empty())
        return from < length_ ? from : length_;
    if (data_ == nullptr || needle.data_ == nullptr || needle.length_ > length_)
        return npos;

    const auto max_start = length_ - needle.length_;
    ca::usize i = from < max_start ? from : max_start;
    for (;;) {
        if (matches_at(data_, length_, needle, i))
            return i;
        if (i == 0)
            break;
        --i;
    }
    return npos;
}

bool Utf16StringRef::starts_with(const Utf16StringRef& prefix) const noexcept
{
    return starts_with(prefix, 0);
}

bool Utf16StringRef::starts_with(const Utf16StringRef& prefix, ca::usize offset) const noexcept
{
    return matches_at(data_, length_, prefix, offset);
}

bool Utf16StringRef::ends_with(const Utf16StringRef& suffix) const noexcept
{
    if (suffix.length() > length_)
        return false;
    return matches_at(data_, length_, suffix, length_ - suffix.length());
}

bool Utf16StringRef::contains(const Utf16StringRef& needle) const noexcept
{
    return index_of(needle) != npos;
}

int Utf16StringRef::compare(const Utf16StringRef& other) const noexcept
{
    const auto common = length_ < other.length_ ? length_ : other.length_;
    for (ca::usize i = 0; i < common; ++i) {
        if (data_[i].unit() < other.data_[i].unit())
            return -1;
        if (data_[i].unit() > other.data_[i].unit())
            return 1;
    }
    if (length_ < other.length_)
        return -1;
    if (length_ > other.length_)
        return 1;
    return 0;
}

bool Utf16StringRef::equals(const Utf16StringRef& other) const noexcept
{
    if (length_ != other.length_)
        return false;
    if (length_ == 0)
        return true;
    if (data_ == nullptr || other.data_ == nullptr)
        return false;
    return std::memcmp(data_, other.data_, length_ * sizeof(Char16)) == 0;
}

ca::i32 Utf16StringRef::hash_code() const noexcept
{
    if (data_ == nullptr)
        return 0;

    ca::u32 hash = 0;
    for (ca::usize i = 0; i < length_; ++i)
        hash = hash * 31u + data_[i].unit();
    return static_cast<ca::i32>(hash);
}

Utf16String Utf16StringRef::concat(const Utf16StringRef& other) const
{
    Utf16StringBuilder builder;
    builder.reserve(length_ + other.length_);
    builder.append(*this);
    builder.append(other);
    return builder.build();
}

Utf16String Utf16StringRef::to_string() const
{
    return Utf16String(data_, length_);
}

std::u16string Utf16StringRef::to_std_u16_string() const
{
    std::u16string out;
    out.reserve(length_);
    for (ca::usize i = 0; i < length_; ++i)
        out.push_back(static_cast<char16_t>(data_[i].unit()));
    return out;
}

Utf8String Utf16StringRef::to_utf8_string() const
{
    if (length_ == 0)
        return Utf8String();

    const auto byte_count = utf16_to_utf8_length(raw_data(), length_);
    if (byte_count == 0)
        throw std::runtime_error("Utf16StringRef::to_utf8_string: invalid UTF-16 sequence");

    std::vector<ca::u8> buffer(byte_count);
    const auto written = utf16_to_utf8(raw_data(), length_, buffer.data());
    if (written != byte_count)
        throw std::runtime_error("Utf16StringRef::to_utf8_string: failed to encode UTF-8");
    return Utf8String(buffer.data(), byte_count);
}

Utf16String::Utf16String() noexcept : data_(nullptr), length_(0) {}

Utf16String::Utf16String(const Char16* data, ca::usize length)
    : data_(nullptr), length_(0)
{
    if (data == nullptr || length == 0)
        return;

    data_ = new Char16[length];
    std::memcpy(data_, data, length * sizeof(Char16));
    length_ = length;
}

Utf16String::Utf16String(const ca::u16* data, ca::usize length)
    : Utf16String(reinterpret_cast<const Char16*>(data), length) {}

Utf16String::Utf16String(const std::u16string& str)
    : Utf16String(reinterpret_cast<const ca::u16*>(str.data()), static_cast<ca::usize>(str.size())) {}

Utf16String::Utf16String(Utf16String&& other) noexcept
    : data_(other.data_), length_(other.length_)
{
    other.data_ = nullptr;
    other.length_ = 0;
}

Utf16String& Utf16String::operator=(Utf16String&& other) noexcept
{
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        length_ = other.length_;
        other.data_ = nullptr;
        other.length_ = 0;
    }
    return *this;
}

Utf16String::~Utf16String()
{
    delete[] data_;
    data_ = nullptr;
}

Utf16String Utf16String::clone() const
{
    return Utf16String(data_, length_);
}

Utf16String Utf16String::from_utf8_string(const Utf8StringRef& str)
{
    if (str.is_empty())
        return Utf16String();

    const auto unit_count = utf8_to_utf16_length(str.data(), str.byte_length());
    if (unit_count == 0)
        throw std::runtime_error("Utf16String::from_utf8_string: invalid UTF-8 sequence");

    std::vector<ca::u16> units(unit_count);
    const auto written = utf8_to_utf16(str.data(), str.byte_length(), units.data());
    if (written != unit_count)
        throw std::runtime_error("Utf16String::from_utf8_string: failed to encode UTF-16");
    return Utf16String(units.data(), unit_count);
}

Utf16String Utf16String::from_code_point(ca::u32 code_point)
{
    Char16 first;
    Char16 second;
    if (!encode_code_point(code_point, first, second))
        throw std::runtime_error("Utf16String::from_code_point: invalid Unicode code point");

    if (code_point < 0x10000)
        return Utf16String(&first, 1);

    Char16 pair[] = {first, second};
    return Utf16String(pair, 2);
}

Utf16String Utf16String::value_of(bool value)
{
    return utf16_from_ascii(value ? "true" : "false");
}

Utf16String Utf16String::value_of(Char16 value)
{
    return Utf16String(&value, 1);
}

Utf16String Utf16String::value_of(ca::i32 value)
{
    return utf16_from_number_text(std::to_string(value));
}

Utf16String Utf16String::value_of(ca::i64 value)
{
    return utf16_from_number_text(std::to_string(value));
}

Utf16String Utf16String::value_of(ca::f32 value)
{
    return value_of_with_stream(value);
}

Utf16String Utf16String::value_of(ca::f64 value)
{
    return value_of_with_stream(value);
}

Utf16String Utf16String::value_of(const char* utf8)
{
    return from_utf8_string(Utf8StringRef::from_cstr(utf8));
}

Utf16String Utf16String::value_of(const Utf8StringRef& utf8)
{
    return from_utf8_string(utf8);
}

Utf16String Utf16String::value_of(const Utf16StringRef& value)
{
    return value.to_string();
}

const ca::u16* Utf16String::raw_data() const noexcept
{
    return reinterpret_cast<const ca::u16*>(data_);
}

Utf16StringBuilder& Utf16StringBuilder::append(Char16 ch)
{
    buffer_.push_back(ch);
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::append(bool value)
{
    return append(Utf16String::value_of(value));
}

Utf16StringBuilder& Utf16StringBuilder::append(ca::i32 value)
{
    return append(Utf16String::value_of(value));
}

Utf16StringBuilder& Utf16StringBuilder::append(ca::i64 value)
{
    return append(Utf16String::value_of(value));
}

Utf16StringBuilder& Utf16StringBuilder::append(ca::f32 value)
{
    return append(Utf16String::value_of(value));
}

Utf16StringBuilder& Utf16StringBuilder::append(ca::f64 value)
{
    return append(Utf16String::value_of(value));
}

Utf16StringBuilder& Utf16StringBuilder::append(const char* utf8)
{
    return append(Utf16String::value_of(utf8));
}

Utf16StringBuilder& Utf16StringBuilder::append(const Utf8StringRef& utf8)
{
    return append(Utf16String::value_of(utf8));
}

Utf16StringBuilder& Utf16StringBuilder::append(const Utf16StringRef& str)
{
    if (str.data() == nullptr || str.length() == 0)
        return *this;
    buffer_.insert(buffer_.end(), str.data(), str.data() + str.length());
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::append(const Utf16String& str)
{
    return append(str.ref());
}

Utf16StringBuilder& Utf16StringBuilder::append(const ca::u16* data, ca::usize length)
{
    return append(Utf16StringRef::from_data(data, length));
}

bool Utf16StringBuilder::append_code_point(ca::u32 code_point)
{
    Char16 first;
    Char16 second;
    if (!encode_code_point(code_point, first, second))
        return false;

    buffer_.push_back(first);
    if (code_point >= 0x10000)
        buffer_.push_back(second);
    return true;
}

Char16 Utf16StringBuilder::char_at(ca::usize index) const noexcept
{
    if (index >= buffer_.size())
        return Char16();
    return buffer_[index];
}

ca::u32 Utf16StringBuilder::code_point_at(ca::usize index) const noexcept
{
    return Utf16StringRef(buffer_.data(), static_cast<ca::usize>(buffer_.size())).code_point_at(index);
}

Utf16StringBuilder& Utf16StringBuilder::set_char_at(ca::usize index, Char16 ch) noexcept
{
    if (index < buffer_.size())
        buffer_[index] = ch;
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, Char16 ch)
{
    if (index > buffer_.size())
        index = static_cast<ca::usize>(buffer_.size());
    buffer_.insert(buffer_.begin() + static_cast<std::ptrdiff_t>(index), ch);
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, bool value)
{
    return insert(index, Utf16String::value_of(value).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, ca::i32 value)
{
    return insert(index, Utf16String::value_of(value).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, ca::i64 value)
{
    return insert(index, Utf16String::value_of(value).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, ca::f32 value)
{
    return insert(index, Utf16String::value_of(value).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, ca::f64 value)
{
    return insert(index, Utf16String::value_of(value).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, const char* utf8)
{
    return insert(index, Utf16String::value_of(utf8).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, const Utf8StringRef& utf8)
{
    return insert(index, Utf16String::value_of(utf8).ref());
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, const Utf16StringRef& str)
{
    if (index > buffer_.size())
        index = static_cast<ca::usize>(buffer_.size());
    if (str.data() == nullptr || str.length() == 0)
        return *this;
    buffer_.insert(buffer_.begin() + static_cast<std::ptrdiff_t>(index),
                   str.data(),
                   str.data() + str.length());
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::insert(ca::usize index, const Utf16String& str)
{
    return insert(index, str.ref());
}

Utf16StringBuilder& Utf16StringBuilder::delete_range(ca::usize begin, ca::usize end)
{
    const auto length = static_cast<ca::usize>(buffer_.size());
    if (begin >= length || begin >= end)
        return *this;
    if (end > length)
        end = length;

    buffer_.erase(buffer_.begin() + static_cast<std::ptrdiff_t>(begin),
                  buffer_.begin() + static_cast<std::ptrdiff_t>(end));
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::delete_char_at(ca::usize index)
{
    if (index >= buffer_.size())
        return *this;
    buffer_.erase(buffer_.begin() + static_cast<std::ptrdiff_t>(index));
    return *this;
}

Utf16StringBuilder& Utf16StringBuilder::reverse()
{
    std::vector<Char16> reversed;
    reversed.reserve(buffer_.size());

    for (ca::usize i = static_cast<ca::usize>(buffer_.size()); i > 0;) {
        const auto current = buffer_[i - 1];
        if (current.is_trail_surrogate() && i >= 2 && buffer_[i - 2].is_lead_surrogate()) {
            reversed.push_back(buffer_[i - 2]);
            reversed.push_back(current);
            i -= 2;
        } else {
            reversed.push_back(current);
            --i;
        }
    }

    buffer_ = std::move(reversed);
    return *this;
}

void Utf16StringBuilder::reserve(ca::usize capacity)
{
    buffer_.reserve(capacity);
}

void Utf16StringBuilder::truncate(ca::usize length) noexcept
{
    if (length < buffer_.size())
        buffer_.resize(length);
}

void Utf16StringBuilder::resize(ca::usize length, Char16 fill)
{
    buffer_.resize(length, fill);
}

Utf16String Utf16StringBuilder::build() const
{
    if (buffer_.empty())
        return Utf16String();
    return Utf16String(buffer_.data(), static_cast<ca::usize>(buffer_.size()));
}

bool operator==(const Utf16StringRef& lhs, const Utf16String& rhs) noexcept
{
    return lhs.equals(rhs.ref());
}

bool operator!=(const Utf16StringRef& lhs, const Utf16String& rhs) noexcept
{
    return !lhs.equals(rhs.ref());
}

bool operator<(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

bool operator>(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept
{
    return lhs.compare(rhs) > 0;
}

bool operator<=(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept
{
    return lhs.compare(rhs) <= 0;
}

bool operator>=(const Utf16StringRef& lhs, const Utf16StringRef& rhs) noexcept
{
    return lhs.compare(rhs) >= 0;
}

std::ostream& operator<<(std::ostream& os, Char16 ch)
{
    const auto flags = os.flags();
    const auto fill = os.fill();
    os << "\\u" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << ch.unit();
    os.flags(flags);
    os.fill(fill);
    return os;
}

std::ostream& operator<<(std::ostream& os, const Utf16StringRef& str)
{
    os << str.to_utf8_string();
    return os;
}

std::ostream& operator<<(std::ostream& os, const Utf16String& str)
{
    os << str.ref();
    return os;
}

}  // namespace ca::str

namespace std {

size_t hash<ca::str::Char16>::operator()(ca::str::Char16 ch) const noexcept
{
    return std::hash<ca::u16>{}(ch.unit());
}

size_t hash<ca::str::Utf16StringRef>::operator()(const ca::str::Utf16StringRef& str) const noexcept
{
    size_t h = 14695981039346656037ULL;
    const auto* data = str.data();
    for (ca::usize i = 0; i < str.length(); ++i) {
        h ^= static_cast<size_t>(data[i].unit() & 0xFF);
        h *= 1099511628211ULL;
        h ^= static_cast<size_t>((data[i].unit() >> 8) & 0xFF);
        h *= 1099511628211ULL;
    }
    return h;
}

size_t hash<ca::str::Utf16String>::operator()(const ca::str::Utf16String& str) const noexcept
{
    return std::hash<ca::str::Utf16StringRef>{}(str.ref());
}

}  // namespace std
