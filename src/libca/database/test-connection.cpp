#include <doctest/doctest.h>
#include <libca/database/connection.hpp>

TEST_CASE("test database connection")
{
    libca::Connection conn;
    std::string       sql = "";
    conn.connect("127.0.0.1", 3306, "root", "123456", "chat");
    conn.update(sql);
}