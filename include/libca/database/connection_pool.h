#ifndef LIBCA_DATABSE_CONNECTION_POOL_H
#define LIBCA_DATABSE_CONNECTION_POOL_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <libca/database/connection.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace libca
{
    // 数据库连接池类
    class ConnectionPool
    {
    public:
        static ConnectionPool & getInstance();

        // 从连接池中获取一个连接
        std::shared_ptr<Connection> getConnection();

    private:
        ConnectionPool();

        bool loadFromConfigFile(const std::string & filename); // 从ini文件加载配置

        void productConnectionTask(); // 生产连接的任务

        void scannerConnectionTask(); // 扫描连接的任务

        std::string m_ip;            // 数据库的ip地址
        uint32_t m_port;             // 数据库的端口
        std::string m_username;      // 数据库的用户名
        std::string m_password;      // 数据库的密码
        std::string m_dbname;        // 数据库的名字
        int32_t m_initSize;          // 连接池的初始连接数量
        int32_t m_maxSize;           // 连接池的最大连接数量
        int32_t m_maxIdleTime;       // 连接池最大空闲时间
        int32_t m_connectionTimeout; // 连接池获取连接的超时时间

        std::queue<Connection *> m_connQueue; // 存储连接的队列
        std::mutex m_queueMutex;              // 维护连接队列的互斥锁
        std::atomic_int m_connectCount;       // 记录连接创建的Connection的数量
        std::condition_variable m_connCV;     // 用于通知生产者线程生产连接
    };

}

#endif // ! LIBCA_DATABSE_CONNECTION_POOL_H
