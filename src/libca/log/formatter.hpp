#ifndef LIBCA_LOG_FORMATTER_H
#define LIBCA_LOG_FORMATTER_H

#include <libca/utility/nocopyable.hpp>
#include <string>

namespace libca::log
{
    // 格式化器
    class Formatter : public libca::utility::nocopyable
    {
    public:
        Formatter() = default;
        virtual ~Formatter() = default;
        virtual std::string format(const std::string & message) = 0;
    };

    // 简单的格式化器
    class SimpleFormatter : public Formatter
    {
    public:
        SimpleFormatter() = default;
        ~SimpleFormatter() override = default;
        std::string format(const std::string & message) override;
    };

}

#endif // !LIBCA_LOG_FORMATTER_H
