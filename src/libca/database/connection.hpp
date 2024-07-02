#ifndef LIBCA_DATABSE_CONNECTION_H
#define LIBCA_DATABSE_CONNECTION_H

#include <cstdint>
#include <ctime>
#include <mysql/mysql.h>
#include <string>
namespace libca {
class Connection
{
public:
    Connection();

    ~Connection();

    bool connect(std::string ip, uint32_t port, std::string username, std::string password,
                 std::string dbname);

    bool update(std::string sql);

    MYSQL_RES* query(std::string sql);

    // 刷新连接的起始的空闲时间点
    void refeshAliveTime();

    // 获取存活的时间
    clock_t getAliveTime();

private:
    MYSQL*  m_mysql;
    clock_t m_aliveTime;   // 进入空闲状态后的时间
};
}   // namespace libca

#endif   // ! LIBCA_DATABSE_CONNECTION_H
