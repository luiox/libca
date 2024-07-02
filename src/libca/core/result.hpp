#ifndef LIBCA_CORE_RESULT_HPP
#define LIBCA_CORE_RESULT_HPP

// 实现一个类似于Rust的Result错误处理机制
#include <exception>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

class Err
{
public:
    Err()  = default;
    ~Err() = default;
    explicit Err(const std::string& str);
    // 将异常信息转为Err
    explicit Err(const std::exception& e);
    // 插入信息到错误信息之后
    Err& append(const std::string str);
    // 将所有错误信息输出为一个字符串
    std::string error();

private:
    std::vector<std::string> m_msg;
};

template<typename T, typename E>
class Result
{
public:
    // 构造函数
    Result(T value)
        : m_isOk(true)
        , m_data(std::move(value))
    {}
    Result(E error)
        : m_isOk(false)
        , m_data(std::move(error))
    {}

    // 检查是否成功
    bool is_ok() const { return m_isOk; }

    // 获取值，如果失败则抛出异常或返回默认值
    T value_or(T default_value) const
    {
        if (m_isOk) {
            return m_data;
        }
        else {
            return default_value;
        }
    }

    // 获取错误，如果成功则抛出异常
    E error() const
    {
        if (!m_isOk) {
            return value_or;
        }
        else {
            throw std::runtime_error("Tried to get error from successful Result");
        }
    }

    T value() const { return m_data; }

private:
    bool               m_isOk;
    std::variant<T, E> m_data;   // 使用std::variant来存储值或错误
};

template<typename T, typename E>
auto ok(T value) -> Result<T, E>
{
    return Result<T, E>(std::move(value));
}

template<typename T, typename E>
auto error(E error) -> Result<T, E>
{
    return Result<T, E>(std::move(error));
}

#endif   // !LIBCA_CORE_RESULT_HPP
