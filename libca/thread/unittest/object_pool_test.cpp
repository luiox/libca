#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "libca/thread/object_pool.hpp"

namespace ca::thread::test {
namespace {

// 简单可校验对象：携带一个可写 payload，用于确认对象真实可用与复用。
struct PooledObject
{
    int payload{0};
};

ObjectPool<PooledObject>::Factory counting_factory(std::atomic<int>& calls)
{
    return [&calls] {
        ++calls;
        return std::make_unique<PooledObject>();
    };
}

TEST(ObjectPoolTest, ReusesReturnedObjectInSingleThread)
{
    std::atomic<int> calls{0};
    ObjectPool<PooledObject> pool(counting_factory(calls));

    auto first = pool.obtain();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(calls.load(), 1);
    first->payload = 42;
    PooledObject* raw = first.get();
    std::weak_ptr<PooledObject> observer = first;

    first.reset();                        // 归还
    EXPECT_EQ(calls.load(), 1);           // 未新建对象
    EXPECT_FALSE(observer.expired());     // 对象被保留在池内

    auto second = pool.obtain();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(calls.load(), 1);           // factory 仍只调用一次
    EXPECT_EQ(second.get(), raw);         // 同一对象真实复用
    EXPECT_EQ(second->payload, 42);       // 未提供钩子时状态保留
}

TEST(ObjectPoolTest, RecycleHookRunsOnReturn)
{
    std::atomic<int> calls{0};
    std::atomic<int> resets{0};
    ObjectPool<PooledObject> pool(
        [&calls] {
            ++calls;
            return std::make_unique<PooledObject>();
        },
        [&](PooledObject& object) {
            ++resets;
            object.payload = 0;
        });

    auto object = pool.obtain();
    object->payload = 7;
    EXPECT_EQ(resets.load(), 0);          // 借出时不触发
    object.reset();
    EXPECT_EQ(resets.load(), 1);          // 归还时触发

    auto again = pool.obtain();
    EXPECT_EQ(calls.load(), 1);           // 复用，未新建
    EXPECT_EQ(again->payload, 0);         // 钩子重置生效
}

TEST(ObjectPoolTest, ShutdownMakesObtainConstructAndRecycleDestroy)
{
    std::atomic<int> calls{0};
    std::atomic<int> resets{0};
    ObjectPool<PooledObject> pool(counting_factory(calls),
                                  [&](PooledObject&) { ++resets; });

    auto pooled = pool.obtain();
    PooledObject* pooled_raw = pooled.get();
    pooled.reset();                       // 入池
    EXPECT_EQ(calls.load(), 1);

    EXPECT_FALSE(pool.is_shutdown());
    pool.shutdown();
    EXPECT_TRUE(pool.is_shutdown());
    pool.shutdown();                      // 幂等

    auto fresh = pool.obtain();
    ASSERT_NE(fresh, nullptr);
    EXPECT_EQ(calls.load(), 2);           // shutdown 后直接构造新对象
    EXPECT_NE(fresh.get(), pooled_raw);   // 不复用池内旧对象
    std::weak_ptr<PooledObject> observer = fresh;
    fresh.reset();
    EXPECT_EQ(resets.load(), 1);          // 归还不再触发钩子
    EXPECT_TRUE(observer.expired());      // 归还改为销毁
}

TEST(ObjectPoolTest, HookExceptionDestroysObjectInsteadOfPooling)
{
    std::atomic<int> calls{0};
    ObjectPool<PooledObject> pool(
        [&calls] {
            ++calls;
            return std::make_unique<PooledObject>();
        },
        [](PooledObject&) { throw std::runtime_error("reset failed"); });

    PooledObject* raw = nullptr;
    {
        auto object = pool.obtain();
        raw = object.get();
        EXPECT_NO_THROW(object.reset());  // 钩子异常被吞掉，不逃出 deleter
    }
    EXPECT_EQ(calls.load(), 1);

    auto next = pool.obtain();
    EXPECT_EQ(calls.load(), 2);           // 坏对象已销毁，重新构造
    EXPECT_NE(next.get(), raw);
}

TEST(ObjectPoolTest, ObjectOutlivingPoolIsDestroyedNotLeaked)
{
    std::shared_ptr<PooledObject> object;
    std::weak_ptr<PooledObject> observer;
    {
        std::atomic<int> calls{0};
        ObjectPool<PooledObject> pool(counting_factory(calls));
        object = pool.obtain();
        observer = object;
    }   // 池先析构
    EXPECT_FALSE(observer.expired());   // 用户仍持有对象
    object.reset();
    EXPECT_TRUE(observer.expired());    // 池死后归还直接销毁，不泄漏
}

TEST(ObjectPoolTest, ConcurrentObtainAndRelease)
{
    std::atomic<int> calls{0};
    ObjectPool<PooledObject> pool(counting_factory(calls));

    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&pool] {
            for (int i = 0; i < 200; ++i) {
                auto object = pool.obtain();   // 高竞争下 try_lock 退化为直接构造
                object->payload = i;
            }   // 立即归还，制造 obtain/recycle 双向竞争
        });
    }
    for (auto& worker : workers)
        worker.join();

    EXPECT_GE(calls.load(), 1);
    auto final_object = pool.obtain();   // 竞争结束后池仍可用
    EXPECT_NE(final_object, nullptr);
}

}  // namespace
}  // namespace ca::thread::test
