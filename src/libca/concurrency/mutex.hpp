#ifndef LIBCA_CONCURRENCY_MUTEX_H
#define LIBCA_CONCURRENCY_MUTEX_H

#include <pthread.h>


namespace libca {
class Mutex
{
public:
    Mutex();
    ~Mutex();

    void lock();

    void unlock();

    int tryLock();

private:
    pthread_mutex_t m_mutex;
};
}   // namespace libca

#endif   // !LIBCA_CONCURRENCY_MUTEX_H
