/**
 * @brief string for libca
 * @author Canrad
 * @date 2023/10/11
 *
 */

#ifndef LIBCA_UTIL_STRING_HPP
#define LIBCA_UTIL_STRING_HPP

#include <iostream>

// using size_t = unsigned int;

namespace libca {
class StringIterator;

class String
{
private:
    char*  m_str;
    size_t m_length;
    size_t m_capacity;

    // 扩容函数
    void expand(size_t new_capacity);

    // 拷贝函数
    void copy(const char* other);

public:
    String();

    explicit String(char val);

    explicit String(int val);

    String(const char* str);

    String(String& str);

    ~String();

    // 获取C风格的字符串指针
    const char* c_str();

    // 获取字符串的长度
    [[nodiscard]] size_t length() const;

    // 获取字符串的字节长度
    [[nodiscard]] size_t byte_length() const;

    // 获取容量
    [[nodiscard]] size_t capacity() const;

    // 改变字符串容量
    bool resize(int capacity);

    // 拷贝出一份新的字符串对象
    String clone();

    // 拼接字符串
    String& append(const char* str);

    String& append(String& str);

    // 插入字符串
    String& insert(const char* str, int index);

    String& insert(String& str, int index);

    // 删除部分字符串内容
    String& erase(int index, int size = 1);

    // 删除全部字符串内容
    String& clear();

    // 替换字符串
    String& replace(const char* find_str, const char* replace_str);

    String& replace(String& find_str, String& replace_str);

    // 查找第一个匹配的字符串，返回其下标，找不到返回-1
    int find(const char* find_str);

    // 获取字符串的子串
    String substr(int begin, int end) const;

    // 消除字符串两端的空格
    String& trim();

    // 比较字符串
    [[nodiscard]] bool compare(const String& other) const;

    // 将字符串中所有字母转小写
    String& to_lower_case();

    // 将字符串中所有字母转大写
    String& to_upper_case();

    // 返回一个起始的迭代器
    StringIterator begin() noexcept;

    // 返回一个末尾的迭代器
    StringIterator end() noexcept;

    String& operator=(const char* other);

    String& operator=(const String& other);

    bool operator==(const String& other);

    char& operator[](int index);

    friend std::ostream& operator<<(std::ostream& out, String& other);
};

////////////////////////////////////////////////////////////////////////////////
class StringIterator
{
private:
    char* ptr;

public:
    StringIterator(char* p);

    char& operator*() const;

    StringIterator& operator++();

    bool operator!=(const StringIterator& other) const;
};
}   // namespace libca


#endif // !LIBCA_UTIL_STRING_HPP
