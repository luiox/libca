#ifndef LIBCA_LOG_SPDLOG_BACKEND_H
#define LIBCA_LOG_SPDLOG_BACKEND_H

#include <atomic>
#include <memory>

#include "logger.hpp"
#include <spdlog/logger.h>

namespace libca {

class SpdlogBackend final : public ILogBackend
{
public:
    explicit SpdlogBackend(std::shared_ptr<spdlog::logger> logger);

    void log(Level level,
             fmt::string_view target,
             fmt::string_view file,
             int line,
             fmt::string_view formatStr,
             fmt::format_args args) override;

    const std::atomic<Level>& get_level_atomic() const override;
    void                      set_level(Level level) override;

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<Level>              levelAtomic_;
};

}   // namespace libca


#endif   // !LIBCA_LOG_SPDLOG_BACKEND_H
