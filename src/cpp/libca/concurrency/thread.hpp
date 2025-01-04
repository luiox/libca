#ifndef LIBCA_CONCURRENCY_THREAD_H
#define LIBCA_CONCURRENCY_THREAD_H

#include <pthread.h>
#include <functional>
#include <memory>

namespace libca {

class Thread
{
public:
    Thread();
    Thread(std::function<void()> func);
    virtual ~Thread() = default;

    virtual void run();

    Thread& start();

    void stop();

    void join();

    static std::unique_ptr<Thread> create(std::function<void()> func);

private:
    static void* runWrapper(void* args);

    pthread_t             m_tid;
    std::function<void()> m_theadFunc;
};

}   // namespace libca

#endif   // !LIBCA_CONCURRENCY_THREAD_H
