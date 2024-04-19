/**
 * @brief string for libca
 * @author Canrad
 * @date 2023/10/11 
 *
 */

#include <cstring>
#include <cctype>
#include <exception>
#include <libca/core/string.h>

namespace libca {
    void string::expand(size_t new_capacity) {
        if (new_capacity <= m_capacity) {
            // 新的容量小于等于当前容量，什么也不做
            return;
        }
        char *new_str = new char[new_capacity]; // 分配新内存
        strcpy(new_str, m_str);                 // 复制原数据到新内存
        delete[] m_str;                         // 释放原内存
        m_str = new_str;                        // 指向新内存
        m_capacity = new_capacity;              // 更新容量值
    }

    void string::copy(const char *other) {
        if (m_str != other) { // 避免自我赋值
            size_t other_length = strlen(other);
            if (other_length >= m_capacity) { // 如果输入字符串长度超过当前容量，进行扩容
                size_t new_capacity = other_length + 1;
                expand(new_capacity);
            }
            strcpy(m_str, other); // 复制输入字符串到当前对象
            m_length = other_length;   // 更新长度值
        }
    }

    string::string()
            : m_str(nullptr),
              m_length(0),
              m_capacity(0) {

    }

    string::string(char val) {
        m_length = 1;
        m_capacity = 2;
        m_str = new char[m_capacity];
        m_str[0] = val;
        m_str[1] = '\0';
    }

    string::string(int val) {
        char buffer[15];
        snprintf(buffer, 15, "%d", val);
        m_length = strlen(buffer);
        m_capacity = m_length + 1;
        m_str = new char[m_capacity];
        strcpy(m_str, buffer);
    }

    string::string(const char *str) {
        m_length = strlen(str);
        m_capacity = m_length + 1;
        m_str = new char[m_capacity];
        strcpy(m_str, str);
    }

    string::string(string &str) {
        size_t length = str.m_length;
        m_length = length;
        m_capacity = m_length + 1;
        m_str = new char[m_capacity];
        strcpy(m_str, str.m_str);
    }

    string::~string() {
        delete[] m_str;
        m_str = nullptr;
    }

    const char *string::c_str() {
        m_str[m_length] = '\0';
        return m_str;
    }

    unsigned char utf8_code_bytes(unsigned char byte) {
        int bytes = 0;

        if (byte <= 0x7F) { // then ASCII 占用1个字节
            bytes = 1;
        } else if (byte >= 0xC0 && byte <= 0xDF) { // then 首字节   UTF-8 占用2个字节
            bytes = 2;
        } else if (byte >= 0xE0 && byte <= 0xEF) { // then 首字节   UTF-8 占用3个字节
            bytes = 3;
        } else if (byte >= 0xF0 && byte <= 0xF7) { // then 首字节   UTF-8 占用4个字节
            bytes = 4;
        } else if (byte >= 0xF8 && byte <= 0xFB) { // then 首字节   UTF-8 占用5个字节
            bytes = 5;
        } else if (byte >= 0xFC && byte <= 0xFD) { // then 首字节   UTF-8 占用6个字节
            bytes = 6;
        } else if (byte > 0x7F && byte < 0xC0) { // then UTF-8   非首字节
            bytes = 0;
        }

        return bytes;
    }

    size_t string::length() const {
        int length = 0;
        for (size_t i = 0; i < m_length;) {
            length += 1;
            i += utf8_code_bytes(m_str[i]);
        }
        return length;
    }

    size_t string::byte_length() const {
        return m_length;
    }

    size_t string::capacity() const {
        return m_capacity;
    }

    bool
    string::resize(int capacity) {
        if (capacity < 0 || capacity < m_length + 1) {
            return false;
        }
        if (m_length == 0) {
            auto new_str = new char[capacity];
            delete m_str;
            m_str = new_str;
            m_capacity = capacity;
        } else {
            expand(capacity);
        }
        return true;
    }

    string &string::operator=(const char *other) {
        copy(other);
        return *this;
    }

    string &string::operator=(const string &other) {
        copy(other.m_str);
        return *this;
    }

    bool string::operator==(const string &other) {
        return compare(other);
    }

    char &string::operator[](int index) {
        if (index < 0 || index >= m_length) {
            throw std::out_of_range("Index out of range.");
            //return m_str[0];
        }
        return m_str[index];
    }

    std::ostream &operator<<(std::ostream &out, string &other) {
        std::cout << other.m_str;
        return out;
    }

    string &string::append(const char *str) {
        size_t new_length = m_length + strlen(str); // 计算拼接后的长度

        // 如果当前容量不足以容纳拼接后的字符串，进行扩容
        if (new_length >= m_capacity) {
            // 新容量为原容量和加上新大小后较大的那个，减少扩容的次数。
            auto new_capacity = m_capacity * 2 > new_length ? m_capacity * 2 : new_length + 1;
            expand(new_capacity);
        }

        strcat(m_str, str); // 拼接字符串
        m_length = new_length;   // 更新长度

        return *this;
    }

    string &string::append(string &str) {
        append(str.c_str());
        return *this;
    }

    string &string::insert(const char *str, int index) {
        if (index < 0 || index > m_length) {
            throw std::out_of_range("Index out of range.");
        }
        if (index == m_length - 1) {
            append(str);
        } else {
            auto length = strlen(str);
            if (m_length + length > m_capacity) {
                expand(m_length + length > m_capacity * 2 ? m_length + length
                                                          : m_capacity * 2);
            }
            memmove(m_str + index + length - 1, m_str + index - 1, m_length - index + 1);
            strncpy(m_str + index, str, length);
            m_length += length;
            m_str[m_length] = '\0';
        }
        return *this;
    }

    string &string::insert(string &str, int index) {
        // 预留优化空间，如果需要可以减少一次strlen
        insert(str.c_str(), index);
        return *this;
    }

    string &string::erase(int index, int size) {
        if (index < 0 || index > m_length - 1) {
            throw std::out_of_range("Index out of range.");
        }
        if (size < 0 || size > m_length) {
            throw std::out_of_range("Size out of range.");
        }
        memmove(m_str + index, m_str + index + size, m_length - index - size);
        m_length -= size;
        m_str[m_length] = '\0';
        return *this;
    }

    string &string::clear() {
        m_length = 0;
        m_str[0] = '\0';
        return *this;
    }

    string &string::replace(const char *find_str, const char *replace_str) {
        auto find_size = strlen(find_str);
        auto replace_size = strlen(replace_str);
        auto pos = 0;
        for (int i = 0; i < m_length; i++) {
            if (0 == strncmp(m_str, find_str, find_size)) {
                pos = i;
                break;
            }
        }
        if (m_length - find_size + replace_size > m_capacity) {
            expand(m_capacity * 2);
        }
        erase(pos, find_size);
        insert(replace_str, pos);
        return *this;
    }

    string &string::replace(string &find_str, string &replace_str) {
        return replace(find_str.m_str, replace_str.m_str);
    }

    int
    string::find(const char *find_str) {
        auto find_size = strlen(find_str);
        auto pos = -1;
        for (int i = 0; i < m_length; i++) {
            if (0 == strncmp(m_str + i, find_str, find_size)) {
                pos = i;
                break;
            }
        }
        return pos;
    }

    string
    string::substr(int begin, int end) const {
        if (begin < 0 || end < 0 || begin > m_length - 1 || end > m_length - 1
            || begin > end) {
            throw std::out_of_range("Invalid range.");
        }
        auto size = end - begin + 1;
        char *const ret = new char[size + 1];
        strncpy(ret, m_str + begin, size);
        ret[size] = '\0';
        return string(ret);
    }

    string &string::trim() {
        if (m_length == 0) {
            return *this; // 空字符串，无需操作
        }

        auto start = 0;
        int end = static_cast<int>(m_length) - 1;

        // 从开头找到第一个非空白字符
        while (start < m_length && isspace(m_str[start])) {
            start++;
        }

        // 从结尾找到第一个非空白字符
        while (end >= 0 && isspace(m_str[end])) {
            end--;
        }

        // 更新字符串的长度和内容
        m_length = end - start + 1;
        memmove(m_str, m_str + start, m_length);
        m_str[m_length] = '\0';
        return *this;
    }

    bool string::compare(const string &other) const {
        return m_length == other.byte_length() && strcmp(m_str, other.m_str) == 0;
    }

    string &string::to_lower_case() {
        for (int i = 0; i < m_length; i++) {
            if (isalpha(static_cast<unsigned char>(m_str[i]))) {
                if ((m_str[i] & 0xC0) == 0xC0) {
                    // 处理中文字符的情况
                    unsigned int unicodeChar = (m_str[i] & 0x1F) << 6;
                    unicodeChar |= (m_str[i + 1] & 0x3F);
                    unicodeChar = towlower(unicodeChar);
                    m_str[i] = static_cast<char>((unicodeChar >> 6) | 0xC0);
                    m_str[i + 1] = static_cast<char>((unicodeChar & 0x3F) | 0x80);
                    i++; // 跳过下一个字节，因为已经处理过了
                } else {
                    m_str[i] = static_cast<char>(
                            tolower(static_cast<unsigned char>(m_str[i]))); // 处理其他字符
                }
            }
        }
        return *this;
    }

    string &string::to_upper_case() {
        for (int i = 0; i < m_length; i++) {
            if (isalpha(static_cast<unsigned char>(m_str[i]))) {
                if ((m_str[i] & 0xC0) == 0xC0) {
                    // 处理中文字符的情况
                    unsigned int unicodeChar = (m_str[i] & 0x1F) << 6;
                    unicodeChar |= (m_str[i + 1] & 0x3F);
                    unicodeChar = towupper(unicodeChar);
                    m_str[i] = static_cast<char>((unicodeChar >> 6) | 0xC0);
                    m_str[i + 1] = static_cast<char>((unicodeChar & 0x3F) | 0x80);
                    i++; // 跳过下一个字节，因为已经处理过了
                } else {
                    m_str[i] = static_cast<char>(
                            toupper(static_cast<unsigned char>(m_str[i]))); // 处理其他字符
                }
            }
        }

        return *this;
    }

    string_iterator string::begin() noexcept {
        return {m_str};
    }

    string_iterator string::end() noexcept {
        return {m_str + m_length};
    }


////////////////////////////////////////////////////////////////////////////////
    string_iterator::string_iterator(char *p) : ptr(p) {}

    char &string_iterator::operator*() const {
        return *ptr;
    }

    string_iterator &string_iterator::operator++() {
        ++ptr;
        return *this;
    }

    bool string_iterator::operator!=(const string_iterator &other) const {
        return ptr != other.ptr;
    }
}

