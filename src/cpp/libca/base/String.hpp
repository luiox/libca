//
// @brief string for libca
// @author Canrad
// @date 2023/10/11
//
//

#ifndef LIBCA_BASE_STRING_HPP
#define LIBCA_BASE_STRING_HPP

#include <iostream>
#include <cstdint>

namespace libca {
class StringIterator;

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
    String();

    // explicit String(char val);

    // explicit String(int val);

    // String(const char* str);

    // String(String& str);

    ~String();

    // 从UTF-8字符串创建
    static String createFromUtf8(const char* utf8Str, size_t length);

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

    // 获取字符下标的utf8字符，时间复杂度O(n)
    uint8_t* at(size_t index);

    // 获取字节下标的字节，时间复杂度O(1)
    uint8_t byteAt(size_t index);

    // 获取字符开始
    StringIterator begin() noexcept;

    // 获取字符结束
    StringIterator end() noexcept;

    // 拼接字符串
    String& append(String& str);


    // 获取C风格的字符串指针
    // const char* c_str();

    // // 改变字符串容量
    // bool resize(int capacity);

    // // 拷贝出一份新的字符串对象
    // String clone();

    // // 拼接字符串
    // String& append(const char* str);

    // String& append(String& str);

    // // 插入字符串
    // String& insert(const char* str, int index);

    // String& insert(String& str, int index);

    // // 删除部分字符串内容
    // String& erase(int index, int size = 1);

    // // 删除全部字符串内容
    // String& clear();

    // // 替换字符串
    // String& replace(const char* find_str, const char* replace_str);

    // String& replace(String& find_str, String& replace_str);

    // // 查找第一个匹配的字符串，返回其下标，找不到返回-1
    // int find(const char* find_str);

    // // 获取字符串的子串
    // String substr(int begin, int end) const;

    // // 消除字符串两端的空格
    // String& trim();

    // // 比较字符串
    // [[nodiscard]] bool compare(const String& other) const;

    // // 将字符串中所有字母转小写
    // String& to_lower_case();

    // // 将字符串中所有字母转大写
    // String& to_upper_case();

    // // 返回一个起始的迭代器
    // StringIterator begin() noexcept;

    // // 返回一个末尾的迭代器
    // StringIterator end() noexcept;

    // String& operator=(const char* other);

    // String& operator=(const String& other);

    // bool operator==(const String& other);

    // char& operator[](int index);

    // friend std::ostream& operator<<(std::ostream& out, String& other);
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
