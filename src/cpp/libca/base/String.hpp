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
#include <cstdint>
#include <vector>

namespace libca {

class StringIterator;
class CharIterator;
class ByteIterator;

class Char
{
private:
    uint8_t* c_;
    uint8_t  str_[8];

public:
    Char(uint8_t* c);
    // C风格字符串
    uint8_t* cStr();
};

class Chars
{
private:
    uint8_t* begin_;
    uint8_t* end_;

public:
    Chars(uint8_t* begin, uint8_t* end);
    // beigin iterator
    CharIterator begin();
    // end iterator
    CharIterator end();
};

class Bytes
{
private:
    uint8_t* begin_;
    uint8_t* end_;

public:
    Bytes(uint8_t* begin, uint8_t* end);
    // beigin iterator
    ByteIterator begin();
    // end iterator
    ByteIterator end();
};

class CharIterator
{
private:
    uint8_t* str_;

public:
    // 构造函数
    CharIterator(uint8_t* str);
    // 解引用操作符
    Char operator*() const;
    // 前缀自增操作符
    CharIterator& operator++();
    // 相等比较操作符
    bool operator==(const CharIterator& other) const;
    // 不相等比较操作符
    bool operator!=(const CharIterator& other) const;
};

class ByteIterator
{
private:
    uint8_t* ptr_;

public:
    ByteIterator(uint8_t* ptr);
    // 解引用操作符
    uint8_t operator*() const;
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
size_t BytesInUtf8Char(unsigned char firstByte);

class SmallString
{
private:
    size_t  length_;
    size_t  byteLength;
    uint8_t buffer_[ShortStrMaxLen];

public:
    SmallString();
    SmallString(uint8_t* bytes, size_t len);
    // 导出为C风格字符串
    [[nodiscard]] const char* cStr();
};

class String
{
private:
    // UTF-8的字符个数（不是字节个数），不包括结束符
    size_t length_;
    // 字符串的字节个数，要始终保证字符串的字节个数小于字符串的最大字节容量，因为导出c风格时候需要加\0
    size_t byteLength_;
    // 字符串的最大字节容量
    size_t capacity_;
    // 字符串缓冲区的指针
    uint8_t* buffer_;
    // 扩容函数
    void expand(size_t capacity);

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
    static String createFromUtf8(uint8_t* utf8Str, size_t length);
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
    [[nodiscard]] size_t length() const;
    // 获取字符串的字节长度
    [[nodiscard]] size_t byteLength() const;
    // 获取容量
    [[nodiscard]] size_t capacity() const;
    // 判断是否为空
    [[nodiscard]] bool isEmpty() const;
    // []获取的是字节下标的字节，时间复杂度O(1)
    [[nodiscard]] uint8_t* operator[](int index);
    // 获取字节下标的字节，时间复杂度O(1)
    [[nodiscard]] uint8_t* at(size_t index);
    // 获取字符下标的utf8字符，时间复杂度O(n)
    [[nodiscard]] uint8_t* atU(size_t index);
    // 以字符单位遍历字符串
    [[nodiscard]] Chars chars() noexcept;
    // 以字节单位遍历字符串
    [[nodiscard]] Bytes bytes() noexcept;
    // 切片，返回一个子字符串，基于字节下标
    [[nodiscard]] Chars slice(size_t start, size_t end) noexcept;
    // 切片，返回一个子字符串，基于字符下标
    [[nodiscard]] Chars sliceU(size_t start, size_t end) noexcept;
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
class StringIterator
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
}   // namespace libca


#endif   // !LIBCA_BASE_STRING_HPP
