#include <gmock/gmock.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>
#include <thread>

#include "libca/core/status.hpp"
#include "libca/process/ipc.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <unistd.h>
#endif

namespace ca::process::test {
namespace {

u64 current_process_id()
{
#if defined(_WIN32)
    return static_cast<u64>(GetCurrentProcessId());
#else
    return static_cast<u64>(getpid());
#endif
}

// ---- SharedMemory ----

TEST(SharedMemoryTest, CreateAndOpenShareMappedBytes)
{
    const std::string name    = "libca_process_shm_" + std::to_string(current_process_id());
    auto              created = ipc::SharedMemory::create(name, 64);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto created_memory = std::move(created).unwrap();
    auto opened         = ipc::SharedMemory::open(name);
    ASSERT_TRUE(opened.is_ok()) << opened.unwrap_err().to_string();
    auto opened_memory = std::move(opened).unwrap();

    std::memcpy(created_memory.data(), "shared", 7);
    EXPECT_STREQ(static_cast<const char*>(opened_memory.data()), "shared");
}

TEST(SharedMemoryTest, CreateTwiceYieldsAlreadyExists)
{
    const std::string name   = "libca_process_shm_dup_" + std::to_string(current_process_id());
    auto              first  = ipc::SharedMemory::create(name, 16);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    auto second = ipc::SharedMemory::create(name, 16);
    ASSERT_TRUE(second.is_err());
    EXPECT_EQ(second.unwrap_err().code(), ca::core::StatusCode::ALREADY_EXISTS);
}

TEST(SharedMemoryTest, OpenMissingSegmentYieldsNotFound)
{
    auto opened = ipc::SharedMemory::open("libca_process_shm_missing_"
                                          + std::to_string(current_process_id()));
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code(), ca::core::StatusCode::NOT_FOUND);
}

TEST(SharedMemoryTest, LifecycleReflectsOpenState)
{
    ipc::SharedMemory idle;
    EXPECT_FALSE(idle.is_open());
    EXPECT_EQ(idle.data(), nullptr);

    const std::string name = "libca_process_shm_life_" + std::to_string(current_process_id());
    auto              created = ipc::SharedMemory::create(name, 32);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto memory = std::move(created).unwrap();
    EXPECT_TRUE(memory.is_open());
    EXPECT_EQ(memory.size(), static_cast<ca::usize>(32));
    EXPECT_NE(memory.data(), nullptr);

    memory.close();
    EXPECT_FALSE(memory.is_open());
    // 头文件契约：未映射时 data() 返回 nullptr。
    EXPECT_EQ(memory.data(), nullptr);
}

// ---- NamedPipe ----

TEST(NamedPipeTest, ServerAndClientExchangeBytes)
{
    const std::string name   = "libca_process_pipe_" + std::to_string(current_process_id());
    auto              server = ipc::NamedPipeServer::create(name);
    ASSERT_TRUE(server.is_ok()) << server.unwrap_err().to_string();

    std::thread worker([server = std::move(server).unwrap()]() mutable {
        auto connection = server.accept();
        if (!connection.is_ok())
            return;
        std::move(connection).unwrap().write_all("pong");
    });
    auto        client = ipc::NamedPipeClient::connect(name);
    ASSERT_TRUE(client.is_ok()) << client.unwrap_err().to_string();
    auto connection = std::move(client).unwrap();
    char buffer[5]{};
    auto count = connection.read(buffer, 4);
    ASSERT_TRUE(count.is_ok()) << count.unwrap_err().to_string();
    EXPECT_EQ(std::string(buffer, count.unwrap()), "pong");
    worker.join();
}

TEST(NamedPipeTest, CreateTwiceYieldsAlreadyExists)
{
    const std::string name  = "libca_process_pipe_dup_" + std::to_string(current_process_id());
    auto              first = ipc::NamedPipeServer::create(name);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    auto second = ipc::NamedPipeServer::create(name);
    ASSERT_TRUE(second.is_err());
    EXPECT_EQ(second.unwrap_err().code(), ca::core::StatusCode::ALREADY_EXISTS)
        << second.unwrap_err().to_string();
}

TEST(NamedPipeTest, ConnectToMissingPipeYieldsNotFound)
{
    auto client = ipc::NamedPipeClient::connect("libca_process_pipe_missing_"
                                                + std::to_string(current_process_id()));
    ASSERT_TRUE(client.is_err());
    EXPECT_EQ(client.unwrap_err().code(), ca::core::StatusCode::NOT_FOUND);
}

// 头文件契约：对端关闭后 read 返回 Ok(0) 表示干净 EOF。
TEST(NamedPipeTest, ReadReturnsZeroAfterPeerClose)
{
    const std::string name   = "libca_process_pipe_eof_" + std::to_string(current_process_id());
    auto              server = ipc::NamedPipeServer::create(name);
    ASSERT_TRUE(server.is_ok()) << server.unwrap_err().to_string();
    auto server_value = std::move(server).unwrap();

    std::thread worker([&name]() {
        auto client = ipc::NamedPipeClient::connect(name);
        if (!client.is_ok())
            return;
        auto connection = std::move(client).unwrap();
        connection.write_all("bye");
        connection.close();
    });
    auto accepted = server_value.accept();
    ASSERT_TRUE(accepted.is_ok()) << accepted.unwrap_err().to_string();
    auto connection = std::move(accepted).unwrap();

    char buffer[8]{};
    auto count = connection.read(buffer, sizeof(buffer));
    ASSERT_TRUE(count.is_ok()) << count.unwrap_err().to_string();
    EXPECT_EQ(std::string(buffer, count.unwrap()), "bye");

    auto eof = connection.read(buffer, sizeof(buffer));
    ASSERT_TRUE(eof.is_ok()) << eof.unwrap_err().to_string();
    EXPECT_EQ(eof.unwrap(), static_cast<ca::usize>(0));
    worker.join();
}

// 双工往返：服务端回显，客户端分块写+读，载荷大于管道缓冲以覆盖 write_all 循环。
TEST(NamedPipeTest, DuplexEchoRoundTrip)
{
    const std::string name   = "libca_process_pipe_echo_" + std::to_string(current_process_id());
    auto              server = ipc::NamedPipeServer::create(name);
    ASSERT_TRUE(server.is_ok()) << server.unwrap_err().to_string();
    auto server_value = std::move(server).unwrap();

    std::thread worker([server = std::move(server_value)]() mutable {
        auto accepted = server.accept();
        if (!accepted.is_ok())
            return;
        auto connection = std::move(accepted).unwrap();
        char buffer[4096];
        while (true) {
            auto count = connection.read(buffer, sizeof(buffer));
            if (count.is_err() || count.unwrap() == 0)
                return;
            if (connection.write_all(buffer, count.unwrap()).is_err())
                return;
        }
    });

    auto        client = ipc::NamedPipeClient::connect(name);
    ASSERT_TRUE(client.is_ok()) << client.unwrap_err().to_string();
    auto connection = std::move(client).unwrap();

    const std::string payload(64 * 1024, 'x');
    std::string       echoed;
    echoed.reserve(payload.size());
    char  buffer[4096]{};
    usize sent = 0;
    while (sent < payload.size()) {
        const usize chunk = std::min<usize>(sizeof(buffer), payload.size() - sent);
        ASSERT_TRUE(connection.write_all(payload.data() + sent, chunk).is_ok());
        sent += chunk;
        usize received = 0;
        while (received < chunk) {
            auto count = connection.read(buffer, sizeof(buffer));
            ASSERT_TRUE(count.is_ok()) << count.unwrap_err().to_string();
            ASSERT_GT(count.unwrap(), static_cast<ca::usize>(0));
            echoed.append(buffer, count.unwrap());
            received += count.unwrap();
        }
    }
    EXPECT_EQ(echoed.size(), payload.size());
    EXPECT_EQ(echoed, payload);

    connection.close();
    worker.join();
}

// ---- NamedSemaphore ----

TEST(NamedSemaphoreTest, ReleaseMakesTimedAcquireSucceed)
{
    const std::string name      = "libca_process_sem_" + std::to_string(current_process_id());
    auto              semaphore = ipc::NamedSemaphore::create(name, 0);
    ASSERT_TRUE(semaphore.is_ok()) << semaphore.unwrap_err().to_string();
    auto value = std::move(semaphore).unwrap();

    auto pending = value.try_acquire_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(pending.is_ok()) << pending.unwrap_err().to_string();
    EXPECT_FALSE(pending.unwrap());
    ASSERT_TRUE(value.release().is_ok());
    auto acquired = value.try_acquire_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(acquired.is_ok()) << acquired.unwrap_err().to_string();
    EXPECT_TRUE(acquired.unwrap());
}

TEST(NamedSemaphoreTest, CreateTwiceYieldsAlreadyExists)
{
    const std::string name  = "libca_process_sem_dup_" + std::to_string(current_process_id());
    auto              first = ipc::NamedSemaphore::create(name, 1);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    auto second = ipc::NamedSemaphore::create(name, 1);
    ASSERT_TRUE(second.is_err());
    EXPECT_EQ(second.unwrap_err().code(), ca::core::StatusCode::ALREADY_EXISTS);
}

TEST(NamedSemaphoreTest, OpenMissingSemaphoreYieldsNotFound)
{
    auto opened = ipc::NamedSemaphore::open("libca_process_sem_missing_"
                                            + std::to_string(current_process_id()));
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code(), ca::core::StatusCode::NOT_FOUND);
}

TEST(NamedSemaphoreTest, InitialCountAllowsFiniteAcquires)
{
    const std::string name      = "libca_process_sem_cnt_" + std::to_string(current_process_id());
    auto              semaphore = ipc::NamedSemaphore::create(name, 2);
    ASSERT_TRUE(semaphore.is_ok()) << semaphore.unwrap_err().to_string();
    auto value = std::move(semaphore).unwrap();

    auto first = value.try_acquire_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    EXPECT_TRUE(first.unwrap());
    auto second = value.try_acquire_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(second.is_ok()) << second.unwrap_err().to_string();
    EXPECT_TRUE(second.unwrap());
    auto third = value.try_acquire_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(third.is_ok()) << third.unwrap_err().to_string();
    EXPECT_FALSE(third.unwrap());
}

TEST(NamedSemaphoreTest, AcquireBlocksUntilRelease)
{
    const std::string name      = "libca_process_sem_blk_" + std::to_string(current_process_id());
    auto              semaphore = ipc::NamedSemaphore::create(name, 0);
    ASSERT_TRUE(semaphore.is_ok()) << semaphore.unwrap_err().to_string();
    auto value = std::move(semaphore).unwrap();

    std::thread worker([&value]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        value.release();
    });
    auto acquired = value.acquire();
    worker.join();
    ASSERT_TRUE(acquired.is_ok()) << acquired.to_string();
}

// Windows ReleaseSemaphore 计数参数是 LONG：超限须报错而非截断。
TEST(NamedSemaphoreTest, RejectsReleaseCountBeyondLongMax)
{
    const std::string name = "libca_process_sem_ovf_" + std::to_string(current_process_id());
    auto              semaphore = ipc::NamedSemaphore::create(name, 0);
    ASSERT_TRUE(semaphore.is_ok()) << semaphore.unwrap_err().to_string();
    auto value = std::move(semaphore).unwrap();

    // (std::numeric_limits<long>::max)() 加括号防 windows.h 的 max 宏击穿。
    const auto overflow = static_cast<u32>((std::numeric_limits<long>::max)()) + 1u;
    const auto result   = value.release(overflow);
    EXPECT_TRUE(result.is_err());
    EXPECT_TRUE(ipc::remove_semaphore(name).is_ok());
}

// ---- MessageQueue ----

TEST(MessageQueueTest, SenderDeliversOneWholeMessage)
{
    const std::string name     = "libca_process_mq_" + std::to_string(current_process_id());
    auto              receiver = ipc::MessageQueue::create(name, 64);
    ASSERT_TRUE(receiver.is_ok()) << receiver.unwrap_err().to_string();
    auto sender = ipc::MessageQueue::open(name);
    ASSERT_TRUE(sender.is_ok()) << sender.unwrap_err().to_string();

    auto receiver_value = std::move(receiver).unwrap();
    auto sender_value   = std::move(sender).unwrap();
    ASSERT_TRUE(sender_value.send("message").is_ok());
    auto received = receiver_value.receive_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
    ASSERT_TRUE(received.unwrap().has_value());
    EXPECT_EQ(*received.unwrap(), "message");
}

TEST(MessageQueueTest, ReceiveBlocksUntilSenderDeliversMessage)
{
    const std::string name =
        "libca_process_mq_blocking_" + std::to_string(current_process_id());
    auto receiver = ipc::MessageQueue::create(name, 64);
    ASSERT_TRUE(receiver.is_ok()) << receiver.unwrap_err().to_string();
    auto sender = ipc::MessageQueue::open(name);
    ASSERT_TRUE(sender.is_ok()) << sender.unwrap_err().to_string();

    auto receiver_value = std::move(receiver).unwrap();
    auto sender_value   = std::move(sender).unwrap();
    std::thread worker([sender = std::move(sender_value)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sender.send("blocking");
    });
    auto received = receiver_value.receive();
    worker.join();

    ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
    EXPECT_EQ(received.unwrap(), "blocking");
}

TEST(MessageQueueTest, ReceiveForReturnsEmptyOnTimeout)
{
    const std::string name =
        "libca_process_mq_timeout_" + std::to_string(current_process_id());
    auto receiver = ipc::MessageQueue::create(name, 64);
    ASSERT_TRUE(receiver.is_ok()) << receiver.unwrap_err().to_string();

    auto receiver_value = std::move(receiver).unwrap();
    auto received = receiver_value.receive_for(std::chrono::milliseconds(5));

    ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
    EXPECT_FALSE(received.unwrap().has_value());
}

// 保序 + 消息边界：三条消息依次发送，逐条原样读出，不粘连。
TEST(MessageQueueTest, PreservesMessageBoundariesAndOrder)
{
    const std::string name =
        "libca_process_mq_order_" + std::to_string(current_process_id());
    auto receiver = ipc::MessageQueue::create(name, 64);
    ASSERT_TRUE(receiver.is_ok()) << receiver.unwrap_err().to_string();
    auto sender = ipc::MessageQueue::open(name);
    ASSERT_TRUE(sender.is_ok()) << sender.unwrap_err().to_string();

    auto receiver_value = std::move(receiver).unwrap();
    auto sender_value   = std::move(sender).unwrap();

    ASSERT_TRUE(sender_value.send("first").is_ok());
    ASSERT_TRUE(sender_value.send("second").is_ok());
    ASSERT_TRUE(sender_value.send("third").is_ok());

    const char* expected[] = {"first", "second", "third"};
    for (const char* message : expected) {
        auto received = receiver_value.receive_for(std::chrono::milliseconds(200));
        ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
        ASSERT_TRUE(received.unwrap().has_value());
        EXPECT_EQ(*received.unwrap(), message);
    }
}

TEST(MessageQueueTest, CreateTwiceYieldsAlreadyExists)
{
    const std::string name  = "libca_process_mq_dup_" + std::to_string(current_process_id());
    auto              first = ipc::MessageQueue::create(name, 16);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    auto second = ipc::MessageQueue::create(name, 16);
    ASSERT_TRUE(second.is_err());
    EXPECT_EQ(second.unwrap_err().code(), ca::core::StatusCode::ALREADY_EXISTS)
        << second.unwrap_err().to_string();
}

TEST(MessageQueueTest, OpenMissingQueueYieldsNotFound)
{
    auto opened = ipc::MessageQueue::open("libca_process_mq_missing_"
                                          + std::to_string(current_process_id()));
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code(), ca::core::StatusCode::NOT_FOUND);
}

// ---- remove_* ----

// remove_* 移除名字后 create 同名对象应可重建；不存在的名字幂等成功。
// Windows 上命名对象随句柄回收，remove 恒成功——本测试主要钉住 POSIX 行为。
TEST(IpcRemoveTest, RemoveAllowsRecreateAndIsIdempotent)
{
    const auto id = std::to_string(current_process_id());

    const std::string shm_name = "libca_process_rm_shm_" + id;
    {
        auto created = ipc::SharedMemory::create(shm_name, 16);
        ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    }
    EXPECT_TRUE(ipc::remove_shared_memory(shm_name).is_ok());
    // 名字已移除：重新 create 成功（Windows 上同名对象可能仍被系统延迟回收，
    // 但本进程句柄已随作用域关闭，重建应当成功）。
    {
        auto recreated = ipc::SharedMemory::create(shm_name, 16);
        EXPECT_TRUE(recreated.is_ok());
    }
    EXPECT_TRUE(ipc::remove_shared_memory(shm_name).is_ok());
    // 幂等：再次移除不存在的名字仍成功。
    EXPECT_TRUE(ipc::remove_shared_memory(shm_name).is_ok());

    const std::string sem_name = "libca_process_rm_sem_" + id;
    {
        auto created = ipc::NamedSemaphore::create(sem_name, 1);
        ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    }
    EXPECT_TRUE(ipc::remove_semaphore(sem_name).is_ok());
    EXPECT_TRUE(ipc::remove_semaphore(sem_name).is_ok());

    const std::string mq_name = "libca_process_rm_mq_" + id;
    {
        auto created = ipc::MessageQueue::create(mq_name, 16);
        ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    }
    EXPECT_TRUE(ipc::remove_message_queue(mq_name).is_ok());
    EXPECT_TRUE(ipc::remove_message_queue(mq_name).is_ok());
}

// 非法名字（含 '/'）与 create 同口径拒绝。
TEST(IpcRemoveTest, RemoveRejectsPathLikeName)
{
    EXPECT_FALSE(ipc::remove_shared_memory("a/b").is_ok());
    EXPECT_FALSE(ipc::remove_semaphore("a/b").is_ok());
    EXPECT_FALSE(ipc::remove_message_queue("a/b").is_ok());
}

}   // namespace
}   // namespace ca::process::test
