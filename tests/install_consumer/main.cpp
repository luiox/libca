#include <libca/json/json.hpp>

int main()
{
    auto result =
        ca::json::JsonReader::read(ca::str::Utf8StringRef::from_cstr(R"({"installed":true})"));
    return result.is_ok() ? 0 : 1;
}
