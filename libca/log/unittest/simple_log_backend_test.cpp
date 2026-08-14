#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "libca/log/logger.hpp"
#include "libca/log/simple_log_backend.hpp"

namespace ca::log::test {
namespace {

// 读取整个文件为字符串；文件不存在返回空。
std::string read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return std::string();
    std::string out((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return out;
}

class TempFile
{
public:
    TempFile()
    {
        // tmpnam 给出平台临时目录下的唯一名；再加计数器避免同进程多次构造碰撞。
        static int counter = 0;
        path_ = std::string(std::tmpnam(nullptr)) + "_" + std::to_string(++counter);
    }
    ~TempFile() { std::remove(path_.c_str()); }
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

// 测试用 OpaqueFormat：持固定字符串，不依赖 fmt，便于断言。
class LiteralFormat final : public OpaqueFormat
{
public:
    explicit LiteralFormat(std::string s)
        : s_(std::move(s))
    {}
    void render_to(std::string& out) const override { out += s_; }

private:
    std::string s_;
};

TEST(SimpleLogBackendTest, WritesFormattedMessageToFile)
{
    TempFile tmp;
    SimpleLogConfig cfg;
    cfg.stream        = SimpleLogConfig::Stream::Stderr;
    cfg.file_path     = tmp.path();
    cfg.color         = false;       // 文件不应含颜色码
    cfg.time_format   = "";          // 关闭时间以便断言确定性
    cfg.show_location = true;
    cfg.show_target   = true;

    SimpleLogBackend backend(cfg);
    LiteralFormat    msg("hello world");
    backend.log(Level::Info, "net", "net.cpp", 42, msg);

    std::string content = read_file(tmp.path());
    ASSERT_FALSE(content.empty());
    // 格式：[LEVEL] [target] message (file:line)
    EXPECT_NE(content.find("[Info]"), std::string::npos);
    EXPECT_NE(content.find("[net]"), std::string::npos);
    EXPECT_NE(content.find("hello world"), std::string::npos);
    EXPECT_NE(content.find("(net.cpp:42)"), std::string::npos);
}

TEST(SimpleLogBackendTest, LocationCanBeHidden)
{
    TempFile tmp;
    SimpleLogConfig cfg;
    cfg.file_path     = tmp.path();
    cfg.color         = false;
    cfg.time_format   = "";
    cfg.show_location = false;
    cfg.show_target   = true;

    SimpleLogBackend backend(cfg);
    backend.log(Level::Warn, "fs", "x.cpp", 1, LiteralFormat("oops"));

    std::string content = read_file(tmp.path());
    EXPECT_EQ(content.find("x.cpp"), std::string::npos);  // 不含文件名
    EXPECT_NE(content.find("oops"), std::string::npos);
}

TEST(SimpleLogBackendTest, TargetCanBeHidden)
{
    TempFile tmp;
    SimpleLogConfig cfg;
    cfg.file_path     = tmp.path();
    cfg.color         = false;
    cfg.time_format   = "";
    cfg.show_target   = false;

    SimpleLogBackend backend(cfg);
    backend.log(Level::Error_, "secret", "y.cpp", 7, LiteralFormat("fail"));

    std::string content = read_file(tmp.path());
    EXPECT_EQ(content.find("[secret]"), std::string::npos);
    EXPECT_NE(content.find("fail"), std::string::npos);
}

TEST(SimpleLogBackendTest, AppendsMultipleLines)
{
    TempFile tmp;
    SimpleLogConfig cfg;
    cfg.file_path   = tmp.path();
    cfg.color       = false;
    cfg.time_format = "";
    cfg.show_location = false;
    cfg.show_target   = false;

    SimpleLogBackend backend(cfg);
    backend.log(Level::Info, "t", "f", 1, LiteralFormat("first"));
    backend.log(Level::Error_, "t", "f", 2, LiteralFormat("second"));

    std::string content = read_file(tmp.path());
    SCOPED_TRACE("file content: [" + content + "]");
    // 两行各以 \n 结尾
    EXPECT_NE(content.find("first\n"), std::string::npos);
    EXPECT_NE(content.find("second\n"), std::string::npos);
}

TEST(SimpleLogBackendTest, DefaultConfigValid)
{
    // 默认配置不应抛异常，至少能构造并调用一次。
    SimpleLogBackend backend;
    EXPECT_NO_THROW({ backend.log(Level::Info, "t", "f.cpp", 1, LiteralFormat("ok")); });
}

}  // namespace
}  // namespace ca::log::test
