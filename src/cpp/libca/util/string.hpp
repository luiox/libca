/**
 * @brief string for libca
 * @author Canrad
 * @date 2023/10/11
 *
 */

#ifndef LIBCA_CORE_STRING_H
#define LIBCA_CORE_STRING_H

#include <iostream>

// using size_t = unsigned int;

namespace libca {
class string_iterator;

class string
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
    string();

    explicit string(char val);

    explicit string(int val);

    string(const char* str);

    string(string& str);

    ~string();

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

    // 拼接字符串
    string& append(const char* str);

    string& append(string& str);

    // 插入字符串
    string& insert(const char* str, int index);

    string& insert(string& str, int index);

    // 删除部分字符串内容
    string& erase(int index, int size = 1);

    // 删除全部字符串内容
    string& clear();

    // 替换字符串
    string& replace(const char* find_str, const char* replace_str);

    string& replace(string& find_str, string& replace_str);

    // 查找第一个匹配的字符串，返回其下标，找不到返回-1
    int find(const char* find_str);

    // 获取字符串的子串
    string substr(int begin, int end) const;

    // 消除字符串两端的空格
    string& trim();

    // 比较字符串
    [[nodiscard]] bool compare(const string& other) const;

    // 将字符串中所有字母转小写
    string& to_lower_case();

    // 将字符串中所有字母转大写
    string& to_upper_case();

    // 返回一个起始的迭代器
    string_iterator begin() noexcept;

    // 返回一个末尾的迭代器
    string_iterator end() noexcept;

    string& operator=(const char* other);

    string& operator=(const string& other);

    bool operator==(const string& other);

    char& operator[](int index);

    friend std::ostream& operator<<(std::ostream& out, string& other);
};

////////////////////////////////////////////////////////////////////////////////
class string_iterator
{
private:
    char* ptr;

public:
    string_iterator(char* p);

    char& operator*() const;

    string_iterator& operator++();

    bool operator!=(const string_iterator& other) const;
};
}   // namespace libca

#endif /* !LIBCA_CORE_STRING_H */
