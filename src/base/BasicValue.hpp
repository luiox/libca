#ifndef LIBCA_BASE_BASIC_VALUE_HPP
#define LIBCA_BASE_BASIC_VALUE_HPP

#include "Platform.hpp"
#include <string>

namespace ca {

class LIBCA_API BasicValue
{
public:
    BasicValue();
    BasicValue(bool value);
    BasicValue(int value);
    BasicValue(double value);
    BasicValue(const char* value);
    BasicValue(const std::string& value);
    ~BasicValue();

    BasicValue& operator=(bool value);
    BasicValue& operator=(int value);
    BasicValue& operator=(double value);
    BasicValue& operator=(const char* value);
    BasicValue& operator=(const std::string& value);
    BasicValue& operator=(const BasicValue& value);

    bool operator==(const BasicValue& other);
    bool operator!=(const BasicValue& other);

    bool        asBool();
    int         asInt();
    double      asDouble();
    std::string asString();

private:
    std::string value_;
};


}   // namespace ca

#endif   // !LIBCA_BASE_BASIC_VALUE_HPP
