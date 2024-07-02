#ifndef LIBCA_CORE_RESULT_HPP
#define LIBCA_CORE_RESULT_HPP

// 实现一个类似于Rust的Result错误处理机制
#include <exception>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

struct Void
{};
typedef struct Void Void;

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
        , m_ok(std::move(value))
    {}

    Result(E error)
        : m_isOk(false)
        , m_err(std::move(error))
    {}

    // 检查是否成功
    bool is_ok() const { return m_isOk; }

    // 获取错误，如果成功则抛出异常
    E unwrap_err() const
    {
        if (!m_isOk) {
            return m_err;
        }
        else {
            throw std::runtime_error("Tried to get error from successful Result");
        }
    }

    // 获取值，如果失败则抛出异常
    const T unwrap()
    {
        if (m_isOk) {
            return m_ok;
        }
        throw std::runtime_error("unwrap error");
    }
    // 获取值，如果失败则返回默认值
    const T unwrap_or(T failback)
    {
        if (m_isOk) {
            return m_ok;
        }
        return failback;
    }

private:
    bool m_isOk;
    T    m_ok;
    E    m_err;
};

template<typename T, typename E>
Result<T, E> ok(T value)
{
    return Result<T, E>(std::move(value));
}

template<typename T, typename E>
Result<T, E> error(E error)
{
    return Result<T, E>(std::move(error));
}

// 特化辅助推导
template<typename T>
Result<T, Err> ok(T value)
{
    return Result<T, Err>(std::move(value));
}

template<typename T>
Result<T, Err> error(Err error)
{
    return Result<T, Err>(std::move(error));
}

#endif   // !LIBCA_CORE_RESULT_HPP
