//
// @brief string for libca
// @author Canrad
// @date 2023/10/11
//
//

#include <cctype>
#include <cstring>
#include "String.hpp"
#include <cstdio>

namespace libca {

// 根据UTF-8第一个字节返回该字符的字节数
size_t BytesInUtf8Char(unsigned char firstByte)
{
    if ((firstByte & 0x80) == 0)
        return 1;   // 0xxxxxxx
    if ((firstByte & 0xE0) == 0xC0)
        return 2;   // 110xxxxx
    if ((firstByte & 0xF0) == 0xE0)
        return 3;   // 1110xxxx
    if ((firstByte & 0xF8) == 0xF0)
        return 4;   // 11110xxx
    return 0;       // 错误的UTF-8编码
}

SmallString::SmallString()
    : byteLength(0)
    , length_(0)
{
    
}

SmallString::SmallString(uint8_t* bytes, size_t len)
    : byteLength(len)
{
    size_t i = 0;
    length_ = 0;
    while(i<len){
        length_ ++;
        i += BytesInUtf8Char(bytes[i]);
    }
    memcpy(buffer_, bytes, len);

    // printf("SmallString::SmallString(uint8_t* bytes, size_t len) len=%d, length_=%d\n", len, length_);
}

// 导出为C风格字符串
const char* SmallString::cStr()
{
    buffer_[byteLength] = '\0';
    return reinterpret_cast<const char*>(buffer_);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void String::expand(size_t capacity)
{
    if (capacity <= capacity_) {
        // 新的容量小于等于当前容量，什么也不做
        return;
    }
    auto newStr = new uint8_t[capacity];    // 分配新内存
    memcpy(newStr, buffer_, byteLength_);   // 复制原数据到新内存
    delete[] buffer_;                       // 释放原内存
    buffer_   = newStr;                     // 指向新内存
    capacity_ = capacity;                   // 更新容量值
}

// void String::copy(const char* other)
// {
//     if (m_str != other) {   // 避免自我赋值
//         size_t other_length = strlen(other);
//         if (other_length >= m_capacity) {   // 如果输入字符串长度超过当前容量，进行扩容
//             size_t new_capacity = other_length + 1;
//             expand(new_capacity);
//         }
//         strcpy(m_str, other);      // 复制输入字符串到当前对象
//         m_length = other_length;   // 更新长度值
//     }
// }

String::String()
    : length_(0)
    , byteLength_(0)
    , capacity_(LongStrDefaultLen)
    , buffer_(new uint8_t[LongStrDefaultLen])
{}

// String::String(char val)
// {
//     m_length   = 1;
//     m_capacity = 2;
//     m_str      = new char[m_capacity];
//     m_str[0]   = val;
//     m_str[1]   = '\0';
// }

// String::String(int val)
// {
//     char buffer[15];
//     snprintf(buffer, 15, "%d", val);
//     m_length   = strlen(buffer);
//     m_capacity = m_length + 1;
//     m_str      = new char[m_capacity];
//     strcpy(m_str, buffer);
// }

// String::String(const char* str)
// {
//     m_length   = strlen(str);
//     m_capacity = m_length + 1;
//     m_str      = new char[m_capacity];
//     strcpy(m_str, str);
// }

// String::String(String& str)
// {
//     size_t length = str.m_length;
//     m_length      = length;
//     m_capacity    = m_length + 1;
//     m_str         = new char[m_capacity];
//     strcpy(m_str, str.m_str);
// }

String::~String()
{
    delete[] buffer_;
    buffer_ = nullptr;
}

String String::createFromUtf8(const char* utf8Str, size_t length)
{
    String str;
    size_t unitCnt = 0;
    for(auto i = 0; i < length; ) {
        // printf("i = %d, utf8Str[i] = %X\n", i, utf8Str[i]);
        auto uintLen = BytesInUtf8Char(utf8Str[i]);
        if(uintLen == 0){
            // 有错误的utf8字符
            str.length_=0;
            
            throw std::runtime_error("invalid utf8 char");
            return str;
        }
        str.byteLength_ += uintLen;
        i += uintLen;
        unitCnt++;
    }
    str.length_ = unitCnt;
    if(str.byteLength_ > str.capacity_) {
        str.buffer_ = new uint8_t[str.byteLength_+1];
    }
    memcpy(str.buffer_, utf8Str, length);
    return str;
}

// 获取字符原始数据
uint8_t* String::rawData() const
{
    return buffer_;
}

// 导出为C风格字符串
const char* String::cStr()
{
    buffer_[byteLength_] = '\0';
    return reinterpret_cast<char*>(buffer_);
}

size_t String::length() const
{
    return length_;
}

size_t String::byteLength() const
{
    auto   ptr = buffer_;
    size_t len = 0;
    for (int i = 0; i < length_; i++) {
        auto bytes = BytesInUtf8Char(*ptr);
        len += bytes;
        ptr += bytes;
    }

    return len;
}

size_t String::capacity() const
{
    return capacity_;
}

// 获取字符下标的utf8字符，时间复杂度O(n)
uint8_t* String::at(size_t index)
{
    if(index >= length_) {
        return nullptr;
    }
    size_t unitCnt = 0;
    size_t i = 0;
    while(i < byteLength_) {
        // printf("i = %d, utf8Str[i] = %X\n", i, utf8Str[i]);
        if(unitCnt == index) {
            return &buffer_[i];
        }
        auto unitLen = BytesInUtf8Char(buffer_[i]);  
        i += unitLen;
        unitCnt++;
    }
    return nullptr;
}

// 获取字节下标的字节，时间复杂度O(1)
uint8_t String::byteAt(size_t index)
{
    return buffer_[index];
}

// 拼接字符串
String& String::append(String& str)
{
    if (str.byteLength_ + byteLength_ > capacity_) {
        // 需要扩容
        auto newCapacity = capacity_ * 1.5;
        expand(newCapacity);
    }
    // 拷贝数据
    memcpy(buffer_ + byteLength_, str.buffer_, str.byteLength_);

    return *this;
}

// bool String::resize(int capacity)
// {
//     if (capacity < 0 || capacity < m_length + 1) {
//         return false;
//     }
//     if (m_length == 0) {
//         auto new_str = new char[capacity];
//         delete m_str;
//         m_str      = new_str;
//         m_capacity = capacity;
//     }
//     else {
//         expand(capacity);
//     }
//     return true;
// }

// String String::clone()
// {
//     String str;
//     str.copy(m_str);
//     return str;
// }

// String& String::operator=(const char* other)
// {
//     copy(other);
//     return *this;
// }

// String& String::operator=(const String& other)
// {
//     copy(other.m_str);
//     return *this;
// }

// bool String::operator==(const String& other)
// {
//     return compare(other);
// }

// char& String::operator[](int index)
// {
//     if (index < 0 || index >= m_length) {
//         throw std::out_of_range("Index out of range.");
//         // return m_str[0];
//     }
//     return m_str[index];
// }

// std::ostream& operator<<(std::ostream& out, String& other)
// {
//     std::cout << other.m_str;
//     return out;
// }

// String& String::append(const char* str)
// {
//     size_t new_length = m_length + strlen(str);   // 计算拼接后的长度

//     // 如果当前容量不足以容纳拼接后的字符串，进行扩容
//     if (new_length >= m_capacity) {
//         // 新容量为原容量和加上新大小后较大的那个，减少扩容的次数。
//         auto new_capacity = m_capacity * 2 > new_length ? m_capacity * 2 : new_length + 1;
//         expand(new_capacity);
//     }

//     strcat(m_str, str);      // 拼接字符串
//     m_length = new_length;   // 更新长度

//     return *this;
// }

// String& String::append(String& str)
// {
//     append(str.c_str());
//     return *this;
// }

// String& String::insert(const char* str, int index)
// {
//     if (index < 0 || index > m_length) {
//         throw std::out_of_range("Index out of range.");
//     }
//     if (index == m_length - 1) {
//         append(str);
//     }
//     else {
//         auto length = strlen(str);
//         if (m_length + length > m_capacity) {
//             expand(m_length + length > m_capacity * 2 ? m_length + length : m_capacity * 2);
//         }
//         memmove(m_str + index + length - 1, m_str + index - 1, m_length - index + 1);
//         strncpy(m_str + index, str, length);
//         m_length += length;
//         m_str[m_length] = '\0';
//     }
//     return *this;
// }

// String& String::insert(String& str, int index)
// {
//     // 预留优化空间，如果需要可以减少一次strlen
//     insert(str.c_str(), index);
//     return *this;
// }

// String& String::erase(int index, int size)
// {
//     if (index < 0 || index > m_length - 1) {
//         throw std::out_of_range("Index out of range.");
//     }
//     if (size < 0 || size > m_length) {
//         throw std::out_of_range("Size out of range.");
//     }
//     memmove(m_str + index, m_str + index + size, m_length - index - size);
//     m_length -= size;
//     m_str[m_length] = '\0';
//     return *this;
// }

// String& String::clear()
// {
//     m_length = 0;
//     m_str[0] = '\0';
//     return *this;
// }

// String& String::replace(const char* find_str, const char* replace_str)
// {
//     auto find_size    = strlen(find_str);
//     auto replace_size = strlen(replace_str);
//     auto pos          = 0;
//     for (int i = 0; i < m_length; i++) {
//         if (0 == strncmp(m_str, find_str, find_size)) {
//             pos = i;
//             break;
//         }
//     }
//     if (m_length - find_size + replace_size > m_capacity) {
//         expand(m_capacity * 2);
//     }
//     erase(pos, find_size);
//     insert(replace_str, pos);
//     return *this;
// }

// String& String::replace(String& find_str, String& replace_str)
// {
//     return replace(find_str.m_str, replace_str.m_str);
// }

// int String::find(const char* find_str)
// {
//     auto find_size = strlen(find_str);
//     auto pos       = -1;
//     for (int i = 0; i < m_length; i++) {
//         if (0 == strncmp(m_str + i, find_str, find_size)) {
//             pos = i;
//             break;
//         }
//     }
//     return pos;
// }

// String String::substr(int begin, int end) const
// {
//     if (begin < 0 || end < 0 || begin > m_length - 1 || end > m_length - 1 || begin > end) {
//         throw std::out_of_range("Invalid range.");
//     }
//     auto        size = end - begin + 1;
//     char* const ret  = new char[size + 1];
//     strncpy(ret, m_str + begin, size);
//     ret[size] = '\0';
//     return String(ret);
// }

// String& String::trim()
// {
//     if (m_length == 0) {
//         return *this;   // 空字符串，无需操作
//     }

//     auto start = 0;
//     int  end   = static_cast<int>(m_length) - 1;

//     // 从开头找到第一个非空白字符
//     while (start < m_length && isspace(m_str[start])) {
//         start++;
//     }

//     // 从结尾找到第一个非空白字符
//     while (end >= 0 && isspace(m_str[end])) {
//         end--;
//     }

//     // 更新字符串的长度和内容
//     m_length = end - start + 1;
//     memmove(m_str, m_str + start, m_length);
//     m_str[m_length] = '\0';
//     return *this;
// }

// bool String::compare(const String& other) const
// {
//     return m_length == other.byte_length() && strcmp(m_str, other.m_str) == 0;
// }

// String& String::to_lower_case()
// {
//     for (int i = 0; i < m_length; i++) {
//         if (isalpha(static_cast<unsigned char>(m_str[i]))) {
//             if ((m_str[i] & 0xC0) == 0xC0) {
//                 // 处理中文字符的情况
//                 unsigned int unicodeChar = (m_str[i] & 0x1F) << 6;
//                 unicodeChar |= (m_str[i + 1] & 0x3F);
//                 unicodeChar  = towlower(unicodeChar);
//                 m_str[i]     = static_cast<char>((unicodeChar >> 6) | 0xC0);
//                 m_str[i + 1] = static_cast<char>((unicodeChar & 0x3F) | 0x80);
//                 i++;   // 跳过下一个字节，因为已经处理过了
//             }
//             else {
//                 m_str[i] = static_cast<char>(
//                     tolower(static_cast<unsigned char>(m_str[i])));   // 处理其他字符
//             }
//         }
//     }
//     return *this;
// }

// String& String::to_upper_case()
// {
//     for (int i = 0; i < m_length; i++) {
//         if (isalpha(static_cast<unsigned char>(m_str[i]))) {
//             if ((m_str[i] & 0xC0) == 0xC0) {
//                 // 处理中文字符的情况
//                 unsigned int unicodeChar = (m_str[i] & 0x1F) << 6;
//                 unicodeChar |= (m_str[i + 1] & 0x3F);
//                 unicodeChar  = towupper(unicodeChar);
//                 m_str[i]     = static_cast<char>((unicodeChar >> 6) | 0xC0);
//                 m_str[i + 1] = static_cast<char>((unicodeChar & 0x3F) | 0x80);
//                 i++;   // 跳过下一个字节，因为已经处理过了
//             }
//             else {
//                 m_str[i] = static_cast<char>(
//                     toupper(static_cast<unsigned char>(m_str[i])));   // 处理其他字符
//             }
//         }
//     }

//     return *this;
// }

StringIterator String::begin() noexcept
{
    return StringIterator(buffer_);
}

StringIterator String::end() noexcept
{
    return StringIterator(buffer_ + byteLength_);
}


////////////////////////////////////////////////////////////////////////////////


 StringIterator::StringIterator(uint8_t* p)
     : ptr_(p)
 {}

SmallString StringIterator::operator*() const
 {
     return SmallString(ptr_, BytesInUtf8Char(*ptr_));
 }

 StringIterator& StringIterator::operator++()
 {
    ptr_ += BytesInUtf8Char(*ptr_);
     return *this;
 }

 bool StringIterator::operator!=(const StringIterator& other) const
 {
     return ptr_ != other.ptr_;
 }


}   // namespace libca
