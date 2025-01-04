#include <iostream>
using namespace std;

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <doctest/doctest.h>

class Formatter
{
public:
    virtual ~Formatter() = default;

    // 重载这个函数来格式化日志消息
    virtual std::string format(const std::string& message) = 0;
};

class BasicFormatter : public Formatter
{
public:
    std::string format(const std::string& message) override
    {
        std::stringstream ss;
        ss << prefix << "[" << std::this_thread::get_id()
           << "] "
           //<< "[" << Level::Info << "] "
           << "[" << __FILE__ << ":" << __LINE__ << "] " << message << suffix;
        return ss.str();
    }

    BasicFormatter& addBefore(const std::string& prefix)
    {
        this->prefix = prefix;
        return *this;
    }

    BasicFormatter& addAfter(const std::string& suffix)
    {
        this->suffix = suffix;
        return *this;
    }

    string prefix;
    string suffix;
};

class Logger
{
    std::mutex mtx;
    Formatter* formatter;

public:
    Logger(Formatter* formatter)
        : formatter(formatter)
    {}

    void log(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << formatter->format(message) << std::endl;
    }
};

TEST_CASE("TEST-LOGGER")
{
    // 使用示例
    BasicFormatter* formatter = new BasicFormatter();
    Logger          logger(formatter);

    formatter->addBefore("prefix");
    formatter->addAfter("suffix");

    logger.log("This is a log message.");
    logger.log("This is 12312a log message.  ");

    delete formatter;

    // 输出:
    // prefix[14004421040] [test-logger.cpp:11] This is a log message.
    logger.log("This is 12312a log message.  ");
}
