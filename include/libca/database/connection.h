#ifndef LIBCA_DATABSE_CONNECTION_H
#define LIBCA_DATABSE_CONNECTION_H

#include <cstdint>
#include <mysql/mysql.h>
#include <string>

namespace libca
{
    class Connection
    {
    public:
        Connection();

        ~Connection();

        bool connect(std::string ip,
                     uint32_t port,
                     std::string user,
                     std::string password,
                     std::string dbname);

        bool update(std::string sql);

        MYSQL_RES * query(std::string sql);

    private:
        MYSQL * m_mysql;
    };
}

#endif // ! LIBCA_DATABSE_CONNECTION_H
