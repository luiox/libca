#include <libca/database/connection.h>
#include <libca/log/logger.h>
#include <mysql/mysql.h>

namespace libca
{
    Connection::Connection() { m_mysql = mysql_init(nullptr); }

    Connection::~Connection()
    {
        if (m_mysql != nullptr) {
            mysql_close(m_mysql);
            m_mysql = nullptr;
        }
    }

    bool
    Connection::connect(std::string ip,
                        uint32_t port,
                        std::string username,
                        std::string password,
                        std::string dbname)
    {
        MYSQL * mysql = mysql_real_connect(m_mysql,
                                           ip.c_str(),
                                           username.c_str(),
                                           password.c_str(),
                                           dbname.c_str(),
                                           port,
                                           nullptr,
                                           0);
        return nullptr != mysql;
    }

    bool
    Connection::update(std::string sql)
    {
        if (0 != mysql_query(m_mysql, sql.c_str())) {
            // error("update failed: %s", mysql_error(m_mysql));
            return false;
        }
        return true;
    }

    MYSQL_RES *
    Connection::query(std::string sql)
    {
        if (0 != mysql_query(m_mysql, sql.c_str())) {
            // error("query failed: %s", mysql_error(m_mysql));
            return nullptr;
        }
        return mysql_use_result(m_mysql);
    }

    void
    Connection::refeshAliveTime()
    {
        m_aliveTime = clock();
    }

    clock_t
    Connection::getAliveTime()
    {
        return clock() - m_aliveTime;
    }
}
