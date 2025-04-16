//
// @brief string for libca
// @author Canrad
// @date 2023/10/11
// @update 2025/2/28
// @version 2.0
// @note: 基于UTF-8编码重新设计
// 字符串的编码是非常复杂的，常用就已有的ANSI、GBK、GB2312、UTF8、UTF-16、UTF32、Unicode
// 因为无法确定一个char*、wchar_t*、std::string、std::wstring里面存储的字符串是什么编码，这是造成混乱的根本原因
//

#include "String.hpp"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <cstdarg>
#include <string>
#include <cstring>
#include <codecvt>
#include <libiconv/iconv.h>

namespace ca {




Char::Char(u8char* c)
    : c_(c)
{}

u8char* Char::cStr()
{
    size_t cnt = BytesInUtf8Char(*c_);
    memcpy(str_, c_, cnt);
    str_[cnt] = '\0';
    return str_;
}

////////////////////////////////////////////////////////////////////////////////


CharSequence::CharSequence(u8char* begin, u8char* end)
    : begin_(begin)
    , end_(end)
{}

CharIterator CharSequence::begin()
{
    return CharIterator(begin_);
}

CharIterator CharSequence::end()
{
    return CharIterator(end_);
}

////////////////////////////////////////////////////////////////////////////////

ByteSequence::ByteSequence(uint8_t* begin, uint8_t* end)
    : begin_(begin)
    , end_(end)
{}

ByteIterator ByteSequence::begin()
{
    return ByteIterator(begin_);
}

ByteIterator ByteSequence::end()
{
    return ByteIterator(end_);
}

////////////////////////////////////////////////////////////////////////////////

// 构造函数
CharIterator::CharIterator(uint8_t* str)
    : str_(str)
{}

// 解引用操作符
Char CharIterator::operator*() const
{
    return Char(str_);
}

// 前缀自增操作符
CharIterator& CharIterator::operator++()
{
    ++str_;
    return *this;
}

// 相等比较操作符
bool CharIterator::operator==(const CharIterator& other) const
{
    return str_ == other.str_;
}

// 不相等比较操作符
bool CharIterator::operator!=(const CharIterator& other) const
{
    return str_ != other.str_;
}

////////////////////////////////////////////////////////////////////////////////

ByteIterator::ByteIterator(uint8_t* ptr)
    : ptr_(ptr)
{}

// 解引用操作符
uint8_t ByteIterator::operator*() const
{
    return *ptr_;
}

// 前缀自增操作符
ByteIterator& ByteIterator::operator++()
{
    ++ptr_;
    return *this;
}
// 自减操作符
ByteIterator& ByteIterator::operator--()
{
    --ptr_;
    return *this;
}

// 相等比较操作符
bool ByteIterator::operator==(const ByteIterator& other) const
{
    return ptr_ == other.ptr_;
}

// 不相等比较操作符
bool ByteIterator::operator!=(const ByteIterator& other) const
{
    return ptr_ != other.ptr_;
}

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

SmallString::SmallString()
    : byteLength(0)
    , length_(0)
{}

SmallString::SmallString(uint8_t* bytes, size_t len)
    : byteLength(len)
{
    size_t i = 0;
    length_  = 0;
    while (i < len) {
        length_++;
        i += BytesInUtf8Char(bytes[i]);
    }
    memcpy(buffer_, bytes, len);

    // printf("SmallString::SmallString(uint8_t* bytes, size_t len) len=%d, length_=%d\n", len,
    // length_);
}

// 导出为C风格字符串
const char* SmallString::cStr()
{
    buffer_[byteLength] = '\0';
    return reinterpret_cast<const char*>(buffer_);
}

////////////////////////////////////////////////////////////////////////////////

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

String::String(String&& str) noexcept
{
    length_     = str.length_;
    byteLength_ = str.byteLength_;
    capacity_   = str.capacity_;
    // 转移缓冲区
    buffer_     = str.buffer_;
    str.buffer_ = nullptr;
}

// 拷贝出一份新的字符串对象
[[nodiscard]] String String::clone() noexcept
{
    String str;
    str.length_     = length_;
    str.byteLength_ = byteLength_;
    str.capacity_   = capacity_;
    str.buffer_     = new uint8_t[capacity_];
    memcpy(str.buffer_, buffer_, capacity_);
    return str;
}

String::~String()
{
    delete[] buffer_;
    buffer_ = nullptr;
}

String String::createFromUtf8(uint8_t* utf8Str, size_t length)
{
    String str;
    size_t unitCnt = 0;
    for (auto i = 0; i < length;) {
        // printf("i = %d, utf8Str[i] = %X\n", i, utf8Str[i]);
        auto uintLen = BytesInUtf8Char(utf8Str[i]);
        if (uintLen == 0) {
            // 有错误的utf8字符
            str.length_ = 0;

            throw std::runtime_error("invalid utf8 char");
            return str;
        }
        str.byteLength_ += uintLen;
        i += uintLen;
        unitCnt++;
    }
    str.length_ = unitCnt;
    if (str.byteLength_ > str.capacity_) {
        str.buffer_ = new uint8_t[str.byteLength_ + 1];
    }
    memcpy(str.buffer_, utf8Str, length);
    return str;
}

// 从C风格字符串创建
String String::createFromCStr(const char* cStr)
{
    auto len = strlen(cStr);
    return createFromUtf8(reinterpret_cast<uint8_t*>(const_cast<char*>(cStr)), len);
}

// 从std::string创建
String String::createFromStdString(const std::string& str)
{
    return createFromUtf8(reinterpret_cast<uint8_t*>(const_cast<char*>(str.c_str())), str.length());
}

// 赋值
String& String::operator=(const String& other)
{
    // 避免自赋值
    if (this != &other) {
        // 如果当前容量小于其他字符串的长度，则重新分配内存
        if (byteLength_ < other.byteLength_) {
            delete[] buffer_;
            buffer_ = new uint8_t[other.byteLength_];
        }
        // 复制其他字符串的内容到当前字符串
        memcpy(buffer_, other.buffer_, other.byteLength_);
        length_     = other.length_;
        byteLength_ = other.byteLength_;
        capacity_   = other.capacity_;
    }
    return *this;
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

[[nodiscard]] bool String::isEmpty() const
{
    return byteLength_ == 0;
}

// 获取字节下标的字节，时间复杂度O(1)
[[nodiscard]] uint8_t* String::at(size_t index)
{
    return &buffer_[index];
}

// []获取的是字节下标的字节，时间复杂度O(1)
[[nodiscard]] uint8_t* String::operator[](int index)
{
    return &buffer_[index];
}

// 获取字符下标的utf8字符，时间复杂度O(n)
[[nodiscard]] uint8_t* String::atU(size_t index)
{
    if (index >= length_) {
        return nullptr;
    }
    size_t unitCnt = 0;
    size_t i       = 0;
    while (i < byteLength_) {
        // printf("i = %d, utf8Str[i] = %X\n", i, utf8Str[i]);
        if (unitCnt == index) {
            return &buffer_[i];
        }
        auto unitLen = BytesInUtf8Char(buffer_[i]);
        i += unitLen;
        unitCnt++;
    }
    return nullptr;
}

// 以字符单位遍历字符串
CharSequence String::chars() noexcept
{
    return CharSequence(buffer_, buffer_ + length_);
}

// 以字节单位遍历字符串
ByteSequence String::bytes() noexcept
{
    return ByteSequence(buffer_, buffer_ + byteLength_);
}

// 切片，返回一个子字符串，基于字节下标
[[nodiscard]] CharSequence String::slice(size_t start, size_t end) noexcept
{
    return CharSequence(buffer_ + start, buffer_ + end);
}
// 切片，返回一个子字符串，基于字符下标
[[nodiscard]] CharSequence String::sliceU(size_t start, size_t end) noexcept
{
    return CharSequence(atU(start), atU(end));
}

// 改变字符串容量，如果小于，将会截断字符串，如果增大将会发生拷贝，返回是否发生截短
bool String::resize(size_t capacity) noexcept
{
    if (capacity < byteLength_) {
        // 截断字符串
        byteLength_ = capacity;
        length_     = 0;
        return true;
    }
    if (capacity > capacity_) {
        // 扩容
        auto newBuffer = new uint8_t[capacity];
        memcpy(newBuffer, buffer_, byteLength_);
        delete[] buffer_;
        buffer_   = newBuffer;
        capacity_ = capacity;
        return false;
    }
    return false;
}

// 删除全部字符串内容
String& String::clear() noexcept
{
    byteLength_ = 0;
    length_     = 0;
    return *this;
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

String& String::append(const char* str)
{
    auto len = strlen(str);
    if (len + byteLength_ > capacity_) {
        // 需要扩容
        // 反复循环计算，需要扩容到多大合适
        size_t newCapacity = capacity_ * 1.5;
        while (len + byteLength_ > newCapacity) {
            newCapacity *= 1.5;
        }
        expand(newCapacity);
    }
    // 拷贝数据
    memcpy(buffer_ + byteLength_, str, len);
    // 更新长度
    byteLength_ += len;
    // 计算一下新的有多少个utf8字符
    size_t cnt = 0;
    auto   i   = 0;
    while (i < len) {
        i += BytesInUtf8Char(buffer_[i]);
        cnt++;
    }
    length_ += cnt;
    return *this;
}


std::ostream& operator<<(std::ostream& out, String& other)
{


    std::cout << other.cStr();

    return out;
}

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

// StringIterator String::begin() noexcept
// {
//     return StringIterator(buffer_);
// }

// StringIterator String::end() noexcept
// {
//     return StringIterator(buffer_ + byteLength_);
// }


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

//编码转换，source_charset是源编码，to_charset是目标编码
std::string code_convert(char *source_charset, char *to_charset, const std::string& sourceStr) //sourceStr是源编码字符串
{
	iconv_t cd = iconv_open(to_charset, source_charset);//获取转换句柄，void*类型
	if (cd == 0)
		return "";
 
    size_t inlen = sourceStr.size();
	size_t outlen = 255;
	char* inbuf = (char*)sourceStr.c_str();
	char outbuf[255];//这里实在不知道需要多少个字节，这是个问题
	//char *outbuf = new char[outlen]; 另外outbuf不能在堆上分配内存，否则转换失败，猜测跟iconv函数有关
	memset(outbuf, 0, outlen);
 
	char *poutbuf = outbuf; //多加这个转换是为了避免iconv这个函数出现char(*)[255]类型的实参与char**类型的形参不兼容
	if (iconv(cd, &inbuf, &inlen, &poutbuf,&outlen) == -1)
		return "";
 
    std::string strTemp(outbuf);//此时的strTemp为转换编码之后的字符串
	iconv_close(cd);
	return strTemp;
}
 
//gbk转UTF-8  
std::string GbkToUtf8(const std::string& strGbk)// 传入的strGbk是GBK编码 
{
	return code_convert("gb2312", "utf-8",strGbk);
}
 
//UTF-8转gbk
std::string Utf8ToGbk(const std::string& strUtf8)
{
	return code_convert("utf-8", "gb2312", strUtf8);
}
 
//gbk转unicode,"UCS-2LE"代表unicode小端模式
std::string GbkToUnicode(const std::string& strGbk)// 传入的strGbk是GBK编码 
{
	return code_convert("gb2312", "UCS-2LE",strGbk);
}
 
//unicode转gbk
std::string UnicodeToGbk(const std::string& strGbk)// 传入的strGbk是GBK编码 
{
	return code_convert("UCS-2LE", "gb2312",strGbk);
}

// string转wstring
std::wstring StringConverter::stringToWideString(const std::string& narrowStr)
{
    // 获取宽字符字符串的长度（包括空终止符）
    int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, nullptr, 0);

    // 分配内存来存储宽字符字符串
    wchar_t* wideStr = new wchar_t[wideStrLength];

    // 将窄字符转换为宽字符
    MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, wideStr, wideStrLength);

    // 创建 std::wstring 对象
    std::wstring result(wideStr);

    // 释放内存
    delete[] wideStr;

    return result;
}

// wstring转string
// 注意: 在Windows下将utf16转utf8的std::string是无法正常显示中文的
std::string StringConverter::wideStringToString(const std::wstring& wideStr)
{
    int bufferSize =
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
    return str;
}

// wstring转本地string
// 注意: 本地ansi可以显示中文，但请不要再网络内容传输中使用它，因为不同计算机本地代码页不相同.
std::string StringConverter::wideStringToString2(const std::wstring& wideStr)
{
    int bufferSize =
        WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr,
    nullptr); return str;
}

// wchar_t*转string
std::string StringConverter::wcharToString(const wchar_t* str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(str);
}

// wchar_t*转wstring
std::wstring StringConverter::wcharToWideString(const wchar_t* wcharStr)
{
    // 使用构造函数将 wchar_t* 转换为 std::wstring
    std::wstring wideStr(wcharStr);

    return wideStr;
}

// char*转wchar_t*
std::wstring StringConverter::charToWchar(const char* charStr)
{
    const int charStrLength = strlen(charStr) + 1;   // char 字符串的长度（包括 null 终止符）

    // 计算 wchar_t 字符串所需的缓冲区大小
    const int wcharStrSize = MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, nullptr, 0);

    // 分配 wchar_t 缓冲区
    wchar_t* wcharStr = new wchar_t[wcharStrSize];

    // 执行转换
    MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, wcharStr, wcharStrSize);

    // 将 wchar_t 字符串封装到 std::wstring 类型
    std::wstring result(wcharStr);

    // 释放内存
    delete[] wcharStr;

    return result;
}

// gbk转utf8
std::string StringConverter::gbkToUtf8(const std::string& gbkString)
{
    int          bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
    std::wstring wideString(bufferSize - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize - 1);

    bufferSize =
        WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8String(bufferSize - 1, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize - 1, nullptr, nullptr);

    return utf8String;
}

// gbk转utf8
// std::string GbkToUTF8(const std::string& gbkString)
// {
//     int          bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr,);
//     std::wstring wideString(bufferSize, L'\0');
//     MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize);

//     bufferSize =
//         WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
//     std::string utf8String(bufferSize, '\0');
//     WideCharToMultiByte(
//         CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize, nullptr, nullptr);
//     return utf8String;
// }

// 将 utf8 编码的字符串转换为 GBK 编码
std::string StringConverter::utf8ToGbk(const std::string& utf8String)
{
    int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
    if (bufferSize == 0) {
        // 转换失败，可以根据实际情况进行错误处理
        return "";
    }

    std::wstring wideString(bufferSize, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], bufferSize);

    bufferSize =
        WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        // 转换失败，可以根据实际情况进行错误处理
        return "";
    }

    std::string gbkString(bufferSize, '\0');
    WideCharToMultiByte(
        CP_ACP, 0, wideString.c_str(), -1, &gbkString[0], bufferSize, nullptr, nullptr);

    return gbkString;
}

// 将 utf8 编码的字符串转换为 Unicode 编码
std::wstring StringConverter::utf8ToUnicode(const std::string& utf8String)
{
    int          bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
    std::wstring unicodeString(bufferSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &unicodeString[0], bufferSize);
    return unicodeString;
}

// 本地代码页转std::wstring
std::wstring StringConverter::localCodePageToWstring(const std::string& str)
{
    int wideStrLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (wideStrLen == 0) {
        // 转换失败，可以根据实际情况处理错误
        return L"";
    }

    std::wstring wideStr(wideStrLen, L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideStrLen) == 0) {
        // 转换失败，可以根据实际情况处理错误
        return L"";
    }

    // 去掉末尾的空字符
    wideStr.resize(wideStrLen - 1);

    return wideStr;
}

// 本地代码页转std::string
std::string StringConverter::localCodePageToUtf8(const std::string& localString)
{
    int wideCharLength = MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, nullptr, 0);
    if (wideCharLength == 0) {
        // 转换失败
        return "";
    }

    std::wstring wideString(wideCharLength, L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, 
        localString.c_str(), -1, &wideString[0], wideCharLength) == 0) {
        // 转换失败
        return "";
    }

    int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) {
        // 转换失败
        return "";
    }

    std::string utf8String(utf8Length, '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) ==
        0) {
        // 转换失败
        return "";
    }

    return utf8String;
}

// Unicode转Utf8
std::string StringConverter::unicodeToUtf8(const std::wstring& unicodeString)
{
    int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) {
        // 转换失败
        return "";
    }

    std::string utf8String(utf8Length, '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) ==
        0) {
        // 转换失败
        return "";
    }

    return utf8String;
}

std::u16string StringConverter::mstrToU16str(String& str)
{
    return u"";
}

std::u32string StringConverter::mstrToU32str(String& str)
{
    return U"";
}

////////////////////////////////////////////////////////////////////////////////

// 字符串辅助工具类

std::string StringUtil::toLowerCase(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::string StringUtil::toUpperCase(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

char StringUtil::toChar(const std::string& input)
{
    char              c = 0;
    std::stringstream ss;
    ss << input;
    ss >> c;
    return c;
}

short StringUtil::toShort(const std::string& input)
{
    short             s = 0;
    std::stringstream ss;
    ss << input;
    ss >> s;
    return s;
}

int StringUtil::toInt(const std::string& input)
{
    // return atoi(input.c_str());
    int               i = 0;
    std::stringstream ss;
    ss << input;
    ss >> i;
    return i;
}

long StringUtil::toLong(const std::string& input)
{
    // return atol(input.c_str());
    long              l = 0;
    std::stringstream ss;
    ss << input;
    ss >> l;
    return l;
}

float StringUtil::toFloat(const std::string& input)
{
    float             f = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> f;
    return f;
}

double StringUtil::toDouble(const std::string& input)
{
    // return atof(input.c_str());
    double            d = 0.0;
    std::stringstream ss;
    ss << input;
    ss >> d;
    return d;
}

std::string StringUtil::toString(char c)
{
    std::ostringstream os;
    os << c;
    return os.str();
}

std::string StringUtil::toString(short s)
{
    std::ostringstream os;
    os << s;
    return os.str();
}

std::string StringUtil::toString(int i)
{
    std::stringstream os;
    os << i;
    return os.str();
}

std::string StringUtil::toString(long l)
{
    std::stringstream os;
    os << l;
    return os.str();
}

std::string StringUtil::toString(float f)
{
    std::ostringstream os;
    os << f;
    return os.str();
}

std::string StringUtil::toString(double d)
{
    std::ostringstream os;
    os << d;
    return os.str();
}

std::string StringUtil::trimStart(const std::string& input)
{
    return trimStart(input, " \r\n");
}

std::string StringUtil::trimStart(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return trimStart(input, str.c_str());
}

std::string StringUtil::trimStart(const std::string& input, const char* trims)
{
    std::string delimiter = trims;
    std::string str       = input;
    size_t      found;
    found = str.find_first_not_of(delimiter);
    if (found != std::string::npos)
        str.erase(0, found);
    else
        str.clear();
    return str;
}

std::string StringUtil::trimEnd(const std::string& input)
{
    return trimEnd(input, " \r\n");
}

std::string StringUtil::trimEnd(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return trimEnd(input, str.c_str());
}

std::string StringUtil::trimEnd(const std::string& input, const char* delims)
{
    std::string delimiter = delims;
    std::string str       = input;
    size_t      found;
    found = str.find_last_not_of(delimiter);
    if (found != std::string::npos)
        str.erase(found + 1);
    else
        str.clear();
    return str;
}

std::string StringUtil::trim(const std::string& input)
{
    return trim(input, " \r\n");
}

std::string StringUtil::trim(const std::string& input, char trim)
{
    std::string str;
    str = trim;
    return StringUtil::trim(input, str.c_str());
}

std::string StringUtil::trim(const std::string& input, const char* trims)
{
    std::string str = trimStart(input, trims);
    return trimEnd(str, trims);
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input)
{
    output.clear();
    std::istringstream iss(input);
    copy(std::istream_iterator<std::string>(iss),
         std::istream_iterator<std::string>(),
         back_inserter(output));
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input, char separator)
{
    output.clear();
    std::stringstream ss(input);
    std::string       item;
    while (getline(ss, item, separator)) {
        output.push_back(item);
    }
}

void StringUtil::split(std::vector<std::string>& output, const std::string& input,
                       const std::string& separators)
{
    output.clear();
    size_t last  = 0;
    size_t index = input.find_first_of(separators, last);
    while (index != std::string::npos) {
        std::string str = input.substr(last, index - last);
        output.push_back(str);
        last  = index + 1;
        index = input.find_first_of(separators, last);
    }
    if (index - last > 0) {
        output.push_back(input.substr(last, index - last));
    }
}

std::string StringUtil::join(std::vector<std::string>& input)
{
    return join(input, ' ');
}

std::string StringUtil::join(std::vector<std::string>& input, char separator)
{
    std::ostringstream                 os;
    std::vector<std::string>::iterator it;
    for (it = input.begin(); it != input.end(); it++) {
        os << *it;
        os << separator;
    }
    return os.str();
}

std::string StringUtil::join(std::vector<std::string>& input, const char* separators)
{
    std::stringstream                  os;
    std::vector<std::string>::iterator it;
    for (it = input.begin(); it != input.end(); it++) {
        os << *it;
        os << separators;
    }
    return os.str();
}

std::string StringUtil::capitalize(const std::string& input)
{
    std::ostringstream os;
    int                size = input.size();
    for (int i = 0; i < size; i++) {
        if (i == 0) {
            if (input[i] >= 97 && input[i] <= 122) {
                os << (char)(input[i] - 32);
            }
        }
        else {
            os << input[i];
        }
    }
    return os.str();
}

int strcasecmp(const char* s1, const char* s2)
{
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            break;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int StringUtil::compare(const std::string& strA, const std::string& strB, bool ignoreCase)
{
    if (ignoreCase) {
        return strcasecmp(strA.c_str(), strB.c_str());
    }
    else {
        return strA.compare(strB);
    }
}

std::string StringUtil::format(const char* format, ...)
{
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsnprintf(buf, sizeof(buf), format, arg_ptr);
    va_end(arg_ptr);
    return std::string(buf);
}

bool StringUtil::isNumeric(const std::string& input)
{
    if (input.find_first_not_of("0123456789.") != std::string::npos)
        return false;
    if (count(input.begin(), input.end(), '.') > 1)
        return false;
    return true;
}


}   // namespace ca

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;
using namespace ca;


// #include "String.hpp"
// #include <iostream>
// #include <cstring>
// #include <doctest/doctest.h>
// using namespace std;
// using namespace ca;

// TEST_CASE("CharTest")
// {
//     uint8_t* cstr = (uint8_t*)"中";
//     Char     c1(cstr);
//     REQUIRE(strlen((char*)c1.cStr()) == 3);
// }

// TEST_CASE("CharsTest") {}

// TEST_CASE("CharIteratorTest") {}

// TEST_CASE("BytesTest") {}

// TEST_CASE("ByteIterator")
// {
//     uint8_t      arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
//     ByteIterator it(arr);
//     int          i = 0;
//     for (; it != arr + 10; ++it) {
//         REQUIRE(*it == arr[i]);
//         i++;
//     }
// }

// TEST_CASE("StringTest")
// {
//     String s1;
//     REQUIRE(s1.isEmpty());

//     // 测试构造函数
//     String s2 = String::createFromCStr("test,中文");
//     REQUIRE(s2.length() == 7);
//     REQUIRE(s2.byteLength() == 11);
//     REQUIRE(*s2.at(0) == 't');
//     REQUIRE(*s2.at(1) == 'e');
//     REQUIRE(*s2.at(2) == 's');
//     REQUIRE(*s2.at(3) == 't');
//     REQUIRE(*s2.at(4) == ',');

//     // 测试移动构造
//     String s3 = std::move(s2);
//     REQUIRE(s3.length() == 7);
//     REQUIRE(s3.byteLength() == 11);
//     REQUIRE(*s3.at(0) == 't');
//     REQUIRE(*s3.at(1) == 'e');
//     REQUIRE(*s3.at(2) == 's');

//     // 深拷贝
//     String s4 = s3.clone();
//     REQUIRE(s4.length() == 7);
//     REQUIRE(s4.byteLength() == 11);
//     REQUIRE(*s4.at(0) == 't');
//     REQUIRE(*s4.at(1) == 'e');
//     REQUIRE(s4.rawData() != s3.rawData());

//     // 赋值
//     String s5;
//     REQUIRE(s5.isEmpty());
//     REQUIRE(s5.capacity() == LongStrDefaultLen);
//     s5 = s4;
//     REQUIRE(s5.length() == 7);
//     REQUIRE(s5.byteLength() == 11);
//     REQUIRE(*s5.at(0) == 't');
//     REQUIRE(*s5.at(1) == 'e');
//     REQUIRE(s5.rawData() != s4.rawData());

//     // c风格字符串导出
//     auto cstr = s5.cStr();
//     REQUIRE(cstr[0] == 't');
//     REQUIRE(cstr[1] == 'e');
//     REQUIRE(cstr[11] == '\0');

//     // 测试获取字符下标
//     auto z = s5.atU(5);
//     auto w = s5.atU(6);
//     REQUIRE(memcmp(z, "中", 3) == 0);
//     REQUIRE(memcmp(w, "文", 3) == 0);


//     // auto zstr = "中";
//     // REQUIRE(s2.at(5) == zstr[0]);
//     // REQUIRE(s2.at(6) == zstr[1]);
//     // auto z = s2.atU(5);
//     // REQUIRE(*z == zstr[0]);
//     // REQUIRE(*(z + 1) == zstr[1]);
//     // REQUIRE(*(z+2) == zstr[1]);
// }

// int main_func()
// {
//     const char* str = u8"test,中文";
//     // 打印十六进制
//     printf("str hex: ");
//     for (auto i = 0; i < strlen(str); i++) {
//         printf("%02x ", static_cast<uint8_t>(*(str + i)));
//     }
//     printf("\n");

//     String s1 = String::createFromCStr(str);
//     // 打印十六进制
//     printf("s1 hex: ");
//     for (auto i = 0; i < s1.byteLength(); i++) {
//         printf("%02x ", *s1.at(i));
//     }
//     printf("\n");

//     // printf("s1 hex by iterator: ");
//     // for (auto ch : s1.bytes()) {
//     //     printf("%s ", ch.cStr());
//     // }
//     // printf("\n");

//     // 打印字符
//     printf("s1 ch by at: ");
//     for (auto i = 0; i < s1.length(); i++) {
//         auto ptr = s1.atU(i);
//         auto ch  = String::createFromUtf8(ptr, BytesInUtf8Char(*ptr));
//         printf("%s ", ch.cStr());
//     }
//     printf("\n");

//     printf("s1 ch by iterator: ");
//     for (auto ch : s1.chars()) {
//         printf("%s ", ch.cStr());
//     }
//     printf("\n");

//     cout << "字符串：" << s1.cStr() << endl;
//     cout << "字节个数：" << s1.byteLength() << endl;
//     cout << "字符个数：" << s1.length() << endl;
//     cout << "at(1): " << *s1.atU(1) << endl;
//     cout << "byteAt(1): " << s1.at(1) << endl;
//     auto ch = String::createFromUtf8((uint8_t*)s1.atU(5), 3);
//     cout << "ch:" << ch << endl;
//     printf("at(5): %s\n", ch.cStr());
//     printf("byteAt(5): %X\n", *s1.at(5));



//     return 0;
// }

// TEST_CASE("test string")
// {
//     using std::cout;
//     using std::endl;

//     libca::string str1("12三四");
//     libca::string str2('a');
//     libca::string str3(1234);
//     libca::string str4{1234};
//     libca::string str5{'a'};
//     libca::string str6 = "1234";
//     cout << "str1.c_str = " << str1.c_str() << endl;
//     cout << "str1.length = " << str1.length() << endl;
//     cout << "str1.byte_length = " << str1.byte_length() << endl;
//     cout << "str1.capacity = " << str1.capacity() << endl;
//     str1.resize(20);
//     cout << "after resize, str1.capacity = " << str1.capacity() << endl;
//     str1.append("56").append(str2);
//     cout << "after append, str1 = " << str1 << endl;
//     str1.insert("0", 0);
//     cout << "after insert, str1 = " << str1 << endl;
//     str1.insert(str2, 0);
//     cout << "after insert, str1 = " << str1 << endl;
//     str1.erase(0);
//     cout << "after erase, str1 = " << str1 << endl;
//     str1.erase(0, 2);
//     cout << "after erase, str1 = " << str1 << endl;
//     str1.replace("2", "1");
//     cout << "after replace, str1 = " << str1 << endl;
//     auto str7 = str1.substr(0, 4);
//     cout << "str1.substr(0,4) = " << str7 << endl;
//     str1.insert("     ", 0);
//     str1.append("                 ").append("1");
//     cout << "after insert and append, str1 = " << str1 << endl;
//     str1[str1.byte_length() - 1] = ' ';
//     cout << "after str1[str1.byte_length() - 1]=' ' str1 = " << str1 << endl;
//     cout << "after trim, str1 = " << str1.trim() << endl;
//     cout << "after to_upper_case, str1 = " << str1.to_upper_case() << endl;
//     cout << "after to_lower_case, str1 = " << str1.to_lower_case() << endl;
//     str1.clear();
//     cout << "after clear, str1 = " << str1 << endl;
// }

TEST_CASE(StringConverterTest)
{
    // 先不测试这些，因为无法确定编码

    // // 测试字符串
    // std::string narrowStr = "Hello, 世界!";
    // std::wstring wideStr = L"Hello, 世界!";

    // // string 转 wstring
    // std::wstring convertedWideStr = StringConverter::stringToWideString(narrowStr);
    // ASSERT_TRUE(wcscmp(convertedWideStr.c_str(), L"Hello, 世界!") == 0);

    // // wstring 转 string
    // std::string convertedNarrowStr = StringConverter::wideStringToString(wideStr);
    // ASSERT_TRUE(strcmp(convertedNarrowStr.c_str(),"Hello, 世界!") == 0);

    // // 检查转换是否可逆
    // bool isReversible = (narrowStr == convertedNarrowStr);
    // ASSERT_TRUE(isReversible);
}


TEST_CASE(StringUtilTest)
{
    ASSERT_EQUAL(StringUtil::toLowerCase("HELLO"), "hello");
    ASSERT_EQUAL(StringUtil::toLowerCase("Hello"), "hello");
    ASSERT_EQUAL(StringUtil::toLowerCase("hello"), "hello");

    ASSERT_EQUAL(StringUtil::toUpperCase("hello"), "HELLO");
    ASSERT_EQUAL(StringUtil::toUpperCase("HELLO"), "HELLO");
    ASSERT_EQUAL(StringUtil::toUpperCase("Hello"), "HELLO");

    ASSERT_EQUAL(StringUtil::toChar("hello"), 'h');
    ASSERT_EQUAL(StringUtil::toChar("HELLO"), 'H');

    ASSERT_EQUAL(StringUtil::toShort("12345"), 12345);
    ASSERT_EQUAL(StringUtil::toShort("-12345"), -12345);
}



#endif   // TEST_ENABLE
