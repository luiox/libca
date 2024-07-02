#include <libca/core/result.hpp>
#include <sstream>

Err::Err(const std::exception & e) { m_msg.push_back(e.what()); }

Err::Err(const std::string & str) { m_msg.push_back(str); }

Err &
Err::append(const std::string str)
{
    m_msg.push_back(str);
    return *this;
}

std::string
Err::error()
{
    std::stringstream ss;
    for (const auto & s : m_msg) {
        ss << s;
    }
    return ss.str();
}
