#include <libca/concurrency/thread.hpp>
namespace libca {

Thread::Thread()
    : m_tid(0)
    , m_theadFunc(nullptr)
{}

Thread::Thread(std::function<void()> func)
    : m_tid(0)
    , m_theadFunc(func)
{}

void Thread::run() {}

Thread& Thread::start()
{
    pthread_create(&m_tid, nullptr, &Thread::runWrapper, this);
    return *this;
}

void Thread::stop()
{
    pthread_exit(PTHREAD_CANCELED);
}

void Thread::join()
{
    pthread_join(m_tid, nullptr);
}

void* Thread::runWrapper(void* args)
{
    auto t = static_cast<Thread*>(args);
    if (nullptr == t->m_theadFunc) {
        t->run();
    }
    else {
        t->m_theadFunc();
    }
    return nullptr;
}

std::unique_ptr<Thread> Thread::create(std::function<void()> func)
{
    auto t = std::make_unique<Thread>(func);
    return t;
}

}   // namespace libca
