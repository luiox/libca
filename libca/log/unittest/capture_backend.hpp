#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "libca/log/logger.hpp"

namespace ca::log::test {

/// @brief 测试用后端：捕获所有 log 调用，供断言。线程安全。
class CaptureBackend final : public ILogBackend
{
public:
    struct Entry
    {
        Level         level;
        std::string   target;
        std::string   file;
        int           line;
        std::string   message;
    };

    void log(Level              level,
             std::string_view   target,
             std::string_view   file,
             int                line,
             const OpaqueFormat& message) override
    {
        std::string rendered;
        message.render_to(rendered);
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back({level, std::string(target), std::string(file), line,
                            std::move(rendered)});
    }

    std::vector<Entry> entries()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    std::size_t size()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    std::mutex         mutex_;
    std::vector<Entry> entries_;
};

}  // namespace ca::log::test
