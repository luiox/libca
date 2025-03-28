#include "BasicValue.hpp"
#include <sstream>
#include <string>

namespace ca {
BasicValue::BasicValue() {}

BasicValue::BasicValue(bool value)
{
    *this = value;
}

BasicValue::BasicValue(int value)
{
    *this = value;
}

BasicValue::BasicValue(double value)
{
    *this = value;
}

BasicValue::BasicValue(const char* value)
    : value_(value)
{}

BasicValue::BasicValue(const std::string& value)
    : value_(value)
{}

BasicValue::~BasicValue() {}

BasicValue& BasicValue::operator=(bool value)
{
    value_ = value ? "true" : "false";
    return *this;
}

BasicValue& BasicValue::operator=(int value)
{
    std::stringstream ss;
    ss << value;
    value_ = ss.str();
    return *this;
}

BasicValue& BasicValue::operator=(double value)
{
    std::stringstream ss;
    ss << value;
    value_ = ss.str();
    return *this;
}

BasicValue& BasicValue::operator=(const char* value)
{
    value_ = value;
    return *this;
}

BasicValue& BasicValue::operator=(const std::string& value)
{
    value_ = value;
    return *this;
}

BasicValue& BasicValue::operator=(const BasicValue& value)
{
    value_ = value.value_;
    return *this;
}

bool BasicValue::operator==(const BasicValue& other)
{
    return value_ == other.value_;
}

bool BasicValue::operator!=(const BasicValue& other)
{
    return !(value_ == other.value_);
}

bool BasicValue::asBool()
{
    if (value_ == "true")
        return true;
    else if (value_ == "false")
        return false;
    return false;
}

int BasicValue::asInt()
{
    return std::atoi(value_.c_str());
}

double BasicValue::asDouble()
{
    return std::atof(value_.c_str());
}

std::string BasicValue::asString()
{
    return value_;
}


}   // namespace ca

#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;


TEST_CASE(BasicValueTest)
{
    ca::BasicValue intVal(1);
    ASSERT_EQUAL(1, intVal.asInt());
    intVal = 2;
    ASSERT_EQUAL(2, intVal.asInt());
    ASSERT_EQUAL("2", intVal.asString());
    ca::BasicValue strVal("hello");
    ASSERT_EQUAL("hello", strVal.asString());
    strVal = "world";
    ASSERT_EQUAL("world", strVal.asString());
    strVal = "false";
    ASSERT_EQUAL(false, strVal.asBool());
    strVal = "true";
    ASSERT_EQUAL(true, strVal.asBool());
    strVal = "1.23";
    ASSERT_EQUAL(1.23, strVal.asDouble());
    strVal = "2.34";
    ASSERT_EQUAL(2.34, strVal.asDouble());
}

#endif
