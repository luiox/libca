#include <libca/concurrency/mutex.hpp>

namespace libca {

Mutex::Mutex()
{
    pthread_mutex_init(&m_mutex, nullptr);
}
Mutex::~Mutex()
{
    pthread_mutex_destroy(&m_mutex);
}

void Mutex::lock()
{
    pthread_mutex_lock(&m_mutex);
}

void Mutex::unlock()
{
    pthread_mutex_unlock(&m_mutex);
}

int Mutex::tryLock()
{
    return pthread_mutex_trylock(&m_mutex);
}

}   // namespace libca
