#ifndef LIBCA_CONCURRENCY_CONDITION_H
#define LIBCA_CONCURRENCY_CONDITION_H

#include <pthread.h>


namespace libca {
class Condition
{
public:
    Condition();
    ~Condition();

    void wait();

    void notify();

    void notify_all();

private:
    pthread_cond_t  m_cond;
    pthread_mutex_t m_mutex;
};
}   // namespace libca

#endif   // !LIBCA_CONCURRENCY_CONDITION_H
