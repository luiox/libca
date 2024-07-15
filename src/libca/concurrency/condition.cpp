#include <libca/concurrency/condition.hpp>
namespace libca {
Condition::Condition()
{
    pthread_cond_init(&m_cond, nullptr);
    pthread_mutex_init(&m_mutex, nullptr);
}
Condition::~Condition()
{
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_mutex);
}

void Condition::wait()
{
    pthread_mutex_lock(&m_mutex);
    pthread_cond_wait(&m_cond, &m_mutex);
    pthread_mutex_unlock(&m_mutex);
}

void Condition::notify()
{
    pthread_cond_signal(&m_cond);
}

void Condition::notify_all()
{
    pthread_cond_broadcast(&m_cond);
}

}   // namespace libca