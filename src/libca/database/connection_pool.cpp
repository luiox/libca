#include <chrono>
#include <cstdint>
#include <functional>
#include <libca/database/connection.hpp>
#include <libca/database/connection_pool.hpp>
#include <libca/log/logger.hpp>
#include <memory>
#include <mutex>
#include <thread>

namespace libca {
ConnectionPool::ConnectionPool()
{
    if (!loadFromConfigFile("config.ini")) {
        // error("load config file failed");
        return;
    }

    for (int32_t i = 0; i < m_initSize; i++) {
        auto conn = new Connection();
        conn->connect(m_ip, m_port, m_username, m_password, m_dbname);
        conn->refeshAliveTime();
        m_connQueue.push(conn);
        m_connectCount++;
    }

    // 启动一个新的线程，作为连接的生产者
    std::thread productThread(std::bind(&ConnectionPool::productConnectionTask, this));
    productThread.detach();   // 分离线程

    // 启动一个线程扫描连接池中的连接，如果连接空闲时间超过最大空闲时间m_maxIdleTime，那么就关闭连接
    std::thread scannerThread(std::bind(&ConnectionPool::scannerConnectionTask, this));
    scannerThread.detach();   // 分离线程
}

bool ConnectionPool::loadFromConfigFile(const std::string& filename)
{
    // 从ini文件加载配置
    return true;
}

ConnectionPool& ConnectionPool::getInstance()
{
    static ConnectionPool instance;
    return instance;
}

void ConnectionPool::productConnectionTask()
{
    for (;;) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        while (!m_connQueue.empty()) {
            m_connCV.wait(lock);   // 队列不为空，生产线程进入等待
        }
        // 如果连接池中的连接数量小于最大连接数量，那么就创建新的连接
        if (m_connectCount < m_maxSize) {
            auto conn = new Connection();
            conn->connect(m_ip, m_port, m_username, m_password, m_dbname);
            conn->refeshAliveTime();
            m_connQueue.push(conn);
            m_connectCount++;
        }
        // 通知消费者线程，有新的连接可以使用
        m_connCV.notify_all();
    }
}

std::shared_ptr<Connection> ConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    while (m_connQueue.empty()) {
        // 在指定时间内等待，如果超时则返回nullptr
        if (std::cv_status::timeout ==
            m_connCV.wait_for(lock, std::chrono::milliseconds(m_connectionTimeout))) {
            // 获取连接超时
            if (m_connQueue.empty()) {
                // error("get connection timeout");
                return nullptr;
            }
        }
    }

    /*
     shared_ptr智能指针析构时，会把connection资源直接delete掉，相当于
     调用connection的析构函数，connection就被close掉了。
     这里需要自定义shared_ptr的释放资源的方式，把connection直接归还到queue当中
    */
    std::shared_ptr<Connection> conn(m_connQueue.front(), [&](Connection* conn) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        conn->refeshAliveTime();
        m_connQueue.push(conn);
    });

    m_connQueue.pop();
    // 当最后一个连接被消费的时候进行通知生产者线程生产连接
    if (m_connQueue.empty()) {
        m_connCV.notify_all();
    }
    return conn;
}

// 扫描连接的任务
void ConnectionPool::scannerConnectionTask()
{
    for (;;) {
        // 模拟定时睡眠效果
        std::this_thread::sleep_for(std::chrono::seconds(m_maxIdleTime));
        std::unique_lock<std::mutex> lock(m_queueMutex);
        // 扫描队列
        while (m_connectCount > m_initSize) {
            Connection* conn = m_connQueue.front();
            if (conn->getAliveTime() >= m_maxIdleTime * 1000) {
                // 关闭连接
                m_connQueue.pop();
                delete conn;   // 调用析构函数释放连接
                m_connectCount--;
            }
            else {
                // 如果队头对的连接没有超过最大空闲时间，那么就退出循环
                break;
            }
        }
    }
}
}   // namespace libca
