#include <libca/concurrency/thread.hpp>
#include <libca/concurrency/mutex.hpp>
#include <libca/concurrency/condition.hpp>
#include <doctest/doctest.h>
#include <iostream>
#include <chrono>

using namespace std;
using namespace libca;

int                   g_counter = 0;
unique_ptr<Mutex>     g_lock;
unique_ptr<Condition> g_cond;

class MyThread : public Thread
{
public:
    void run() override
    {
        while (1) {
            g_lock->lock();
            g_counter++;
            cout << "MyThread t1:" << g_counter << endl;
            if (g_counter >= 1000) {
                g_lock->unlock();
                stop();
            }
            g_lock->unlock();
        }
    }
};

TEST_CASE("test Thread")
{
    g_lock = make_unique<Mutex>();
    g_cond = make_unique<Condition>();

    MyThread t1;
    t1.start();
    Thread t2([&t2]() {
        while (1) {
            g_lock->lock();
            g_counter++;
            cout << "Thread t2:" << g_counter << endl;
            if (g_counter >= 3000) {
                t2.stop();
                g_lock->unlock();
            }

            g_lock->unlock();
        }
    });
    t2.start();
    Thread::create([]() {
        while (g_counter <= 5000) {
            g_lock->lock();
            g_counter++;
            cout << "Thread uname:" << g_counter << endl;
            g_lock->unlock();
        }
    })->start();


    cout << "all thread finished !" << endl;
    sleep(10);
}
