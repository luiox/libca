//
// @brief string for libca
// @author Canrad
// @date 2023/10/11
// @update 2025/2/28
// @version 2.0
// @note: 基于UTF-8编码重新设计
//

#ifndef LIBCA_BASE_STRING_HPP
#define LIBCA_BASE_STRING_HPP

#include <iostream>
#include <string>
#include <cstdint>
#include <vector>
#include "Platform.hpp"
#include "libca/base/Platform.hpp"

namespace ca {
    using u8char  = u8;
    using u16char = u16;
    using u32char = u32;

class StringIterator;
class CharIterator;
class ByteIterator;

class LIBCA_API Char
{
private:
    u8char* c_;
    u8char  str_[8];

public:
    explicit Char(u8char* c);
    // C风格字符串
    u8char* cStr();
};

class LIBCA_API CharSequence
{
private:
    u8char* begin_;
    u8char* end_;

public:
    CharSequence(u8char* begin, u8char* end);
    // beigin iterator
    CharIterator begin();
    // end iterator
    CharIterator end();
};

class ByteSequence
{
private:
    u8* begin_;
    u8* end_;

public:
    ByteSequence(u8* begin, u8* end);
    // beigin iterator
    ByteIterator begin();
    // end iterator
    ByteIterator end();
};

class LIBCA_API CharIterator
{
private:
    u8char* str_;

public:
    // 构造函数
    explicit CharIterator(u8char* str);
    // 解引用操作符
    Char operator*() const;
    // 前缀自增操作符
    CharIterator& operator++();
    // 相等比较操作符
    bool operator==(const CharIterator& other) const;
    // 不相等比较操作符
    bool operator!=(const CharIterator& other) const;
};

class LIBCA_API ByteIterator
{
private:
    u8* ptr_;

public:
    explicit ByteIterator(u8* ptr);
    // 解引用操作符
    u8 operator*() const;
    // 前缀自增操作符
    ByteIterator& operator++();
    // 自减操作符
    ByteIterator& operator--();
    // 相等比较操作符
    bool operator==(const ByteIterator& other) const;
    // 不相等比较操作符
    bool operator!=(const ByteIterator& other) const;
};

constexpr static size_t ShortStrMaxLen    = sizeof(void*) * 8 - sizeof(size_t);
constexpr static size_t LongStrDefaultLen = ShortStrMaxLen;

// 根据UTF-8第一个字节返回该字符的字节数
usize LIBCA_API BytesInUtf8Char(unsigned char firstByte);

class LIBCA_API SmallString
{
private:
    usize  length_;
    usize  byteLength;
    usize buffer_[ShortStrMaxLen];

public:
    SmallString();
    SmallString(uint8_t* bytes, size_t len);
    // 导出为C风格字符串
    [[nodiscard]] const char* cStr();
};

class LIBCA_API String
{
private:
    // UTF-8的字符个数（不是字节个数），不包括结束符
    usize length_;
    // 字符串的字节个数，要始终保证字符串的字节个数小于字符串的最大字节容量，因为导出c风格时候需要加\0
    usize byteLength_;
    // 字符串的最大字节容量
    usize capacity_;
    // 字符串缓冲区的指针
    u8char* buffer_;
    // 扩容函数
    void expand(usize capacity);

public:
    // 默认构造函数
    String();
    // 拷贝构造函数已被删除，深拷贝使用clone
    String(String& str) = delete;
    // 移动构造函数，原来来的缓冲区会被清空
    String(String&& str) noexcept;
    // 拷贝出一份新的字符串对象
    [[nodiscard]] String clone() noexcept;
    // 析构函数
    ~String();
    // 从UTF-8字符串创建，utf8Str是utf8编码字符串的字节数组
    static String createFromUtf8(u8char* utf8Str, usize length);
    // 从C风格字符串创建
    static String createFromCStr(const char* cStr);
    // 从std::string创建
    static String createFromStdString(const std::string& str);
    // 赋值
    String& operator=(const String& other);
    // 获取字符原始数据
    [[nodiscard]] uint8_t* rawData() const;
    // 导出为C风格字符串
    [[nodiscard]] const char* cStr();
    // 获取字符串的长度
    [[nodiscard]] usize length() const;
    // 获取字符串的字节长度
    [[nodiscard]] usize byteLength() const;
    // 获取容量
    [[nodiscard]] usize capacity() const;
    // 判断是否为空
    [[nodiscard]] bool isEmpty() const;
    // []获取的是字节下标的字节，时间复杂度O(1)
    [[nodiscard]] u8char* operator[](int index);
    // 获取字节下标的字节，时间复杂度O(1)
    [[nodiscard]] u8* at(size_t index);
    // 获取字符下标的utf8字符，时间复杂度O(n)
    [[nodiscard]] u8char* atU(size_t index);
    // 以字符单位遍历字符串
    [[nodiscard]] CharSequence chars() noexcept;
    // 以字节单位遍历字符串
    [[nodiscard]] ByteSequence bytes() noexcept;
    // 切片，返回一个子字符串，基于字节下标
    [[nodiscard]] CharSequence slice(size_t start, size_t end) noexcept;
    // 切片，返回一个子字符串，基于字符下标
    [[nodiscard]] CharSequence sliceU(size_t start, size_t end) noexcept;
    // 改变字符串容量，如果小于，将会截断字符串，如果增大将会发生拷贝，返回是否发生截短
    bool resize(size_t capacity) noexcept;
    // 删除全部字符串内容
    String& clear() noexcept;
    // 拼接字符串
    String& append(const char* str);
    String& append(String& str);
    // 插入字符串，这个下标是字节下标
    String& insert(const char* str, int index);
    String& insert(String& str, int index);
    // 插入字符串，这个下标是字符下标
    String& insertU(const char* str, int index);
    // 删除部分字符串内容
    String& erase(int index, int size = 1);
    // 替换字符串，基于字节下标
    String& replace(const char* find_str, const char* replace_str);
    String& replace(String& find_str, String& replace_str);
    // 替换字符串，这个下标是字符下标
    String& replaceU(const char* find_str, const char* replace_str);
    // 查找第一个匹配的字符串，返回其字节下标，找不到返回-1
    [[nodiscard]] int find(const char* find_str);
    // 查找第一个匹配的字符串，返回其字符下标，找不到返回-1
    [[nodiscard]] int findU(const char* find_str);
    // 反向查找，字节下标，因为utf8编码，所以反向查找很麻烦
    [[nodiscard]] int rfind(const char* find_str);
    // 获取字符串的子串，基于字节下标
    [[nodiscard]] String substr(int begin, int end) const;
    // 获取字符串的子串，基于字符下标
    [[nodiscard]] String substrU(int begin, int end) const;
    // 消除字符串两端的空格
    String& trim();
    // 将字符串中所有字母转小写
    String& toLowerCase();
    // 将字符串中所有字母转大写
    String& toUpperCase();
    // split分割字符串
    [[nodiscard]] std::vector<String> split(const char* split_str);
    // 反向分割
    [[nodiscard]] std::vector<String> rsplit(const char* split_str);
    // 比较两个字符串的内容是否相等
    [[nodiscard]] bool operator==(const String& other);
    // 比较两个字符串内部的指针是否相等
    [[nodiscard]] bool equals(const String* other);
    // 比较两个字符串的内容是否不相等
    [[nodiscard]] bool operator!=(const String& other);
    // 打印字符串
    friend std::ostream& operator<<(std::ostream& out, String& other);
};

////////////////////////////////////////////////////////////////////////////////
class LIBCA_API StringIterator
{
private:
    uint8_t* ptr_;

public:
    StringIterator(uint8_t* p);

    uint8_t* raw() const;

    SmallString operator*() const;

    StringIterator& operator++();

    bool operator!=(const StringIterator& other) const;
};

////////////////////////////////////////////////////////////////////////////////

// 字符串在不同平台上的一些处理，尤其是Window平台，处理各种字符串的转换
// 这个类纯粹是一个工具类，不包含任何数据
class LIBCA_API StringConverter
{
public:
    StringConverter() = delete;
    ~StringConverter() = delete;

    // string转wstring
    static std::wstring stringToWideString(const std::string& narrowStr);

    // wstring转string
    static std::string wideStringToString(const std::wstring& wideStr);

    // wstring转本地string
    static std::string wideStringToString2(const std::wstring& wideStr);

    // wchar_t*转string
    static std::string wcharToString(const wchar_t* str);

    // wchar_t*转wstring
    static std::wstring wcharToWideString(const wchar_t* wcharStr);

    // char*转wchar_t*
    static std::wstring charToWchar(const char* charStr);

    // gbk转utf8
    static std::string gbkToUtf8(const std::string& gbkString);

    // 将 utf8 编码的字符串转换为 GBK 编码
    static std::string utf8ToGbk(const std::string& utf8String);

    // 将 utf8 编码的字符串转换为 Unicode 编码
    static std::wstring utf8ToUnicode(const std::string& utf8String);

    // Unicode转Utf8
    static std::string unicodeToUtf8(const std::wstring& unicodeString);

    // 本地代码页转std::wstring
    static std::wstring localCodePageToWstring(const std::string& str);

    // 本地代码页转std::string
    static std::string localCodePageToUtf8(const std::string& localString);

    static std::u16string mstrToU16str(String& str);

    static std::u32string mstrToU32str(String& str);
};

////////////////////////////////////////////////////////////////////////////////

// 字符串辅助工具类

class StringUtil
{
public:
    static std::string toLowerCase(const std::string& input);
    static std::string toUpperCase(const std::string& input);

    static char   toChar(const std::string& input);
    static short  toShort(const std::string& input);
    static int    toInt(const std::string& input);
    static long   toLong(const std::string& input);
    static float  toFloat(const std::string& input);
    static double toDouble(const std::string& input);

    static std::string toString(char c);
    static std::string toString(short s);
    static std::string toString(int i);
    static std::string toString(long l);
    static std::string toString(float f);
    static std::string toString(double d);

    static std::string trimStart(const std::string& input);
    static std::string trimStart(const std::string& input, char trim);
    static std::string trimStart(const std::string& input, const char* trims);

    static std::string trimEnd(const std::string& input);
    static std::string trimEnd(const std::string& input, char trim);
    static std::string trimEnd(const std::string& input, const char* trims);

    static std::string trim(const std::string& input);
    static std::string trim(const std::string& input, char trim);
    static std::string trim(const std::string& input, const char* trims);

    static void split(std::vector<std::string>& output, const std::string& input);
    static void split(std::vector<std::string>& output, const std::string& input, char separator);
    static void split(std::vector<std::string>& output, const std::string& input,
                      const std::string& separators);

    static std::string join(std::vector<std::string>& input);
    static std::string join(std::vector<std::string>& input, char separator);
    static std::string join(std::vector<std::string>& input, const char* separators);

    static std::string capitalize(const std::string& input);

    static int compare(const std::string& strA, const std::string& strB, bool ignoreCase = false);
    static std::string format(const char* format, ...);

    static bool isNumeric(const std::string& input);
};

}   // namespace ca


#endif   // !LIBCA_BASE_STRING_HPP
