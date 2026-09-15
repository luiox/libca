#include <gmock/gmock.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include "libca/process/ipc.hpp"
#include "libca/process/subprocess.hpp"

// ---- 多进程 IPC 容错测试的子进程角色（由 ipc_ring_test.cpp 自 spawn） ----

// 写者角色：发送 count 条 "msg-<i>"。用 _Exit 模拟「进程退出但不做队列清理」——
// 写者身份留在头部，读者据此获得「写者已死亡且排空」的 UNAVAILABLE 终止条件
// （若走正常析构，close() 会优雅释放身份，读者只会一直等待新写者）。
static int ring_writer_child(const char* name, const char* count_text)
{
    auto opened = ca::process::ipc::ShmRingQueue::open(name);
    if (opened.is_err())
        return 3;
    auto queue = std::move(opened).unwrap();
    const int count = std::atoi(count_text);
    for (int index = 0; index < count; ++index) {
        if (queue.send("msg-" + std::to_string(index)).is_err())
            return 4;
    }
    std::_Exit(0);
}

// 挂住的写者角色：发送 count 条后休眠 hold_ms，由父进程 kill 模拟崩溃。
static int ring_writer_hang_child(const char* name, const char* count_text, const char* hold_text)
{
    auto opened = ca::process::ipc::ShmRingQueue::open(name);
    if (opened.is_err())
        return 3;
    auto queue = std::move(opened).unwrap();
    const int count = std::atoi(count_text);
    for (int index = 0; index < count; ++index) {
        if (queue.send("msg-" + std::to_string(index)).is_err())
            return 4;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(hold_text)));
    std::_Exit(0);
}

// 死于写中的写者角色：发送一条完整消息后直接构造撕裂槽（Writing 态 + 半截 payload），
// 再 _Exit 让写者进程消失——读者必须能跳过该槽而不读到撕裂数据。
static int ring_writer_torn_child(const char* name)
{
    auto opened = ca::process::ipc::ShmRingQueue::open(name);
    if (opened.is_err())
        return 3;
    auto queue = std::move(opened).unwrap();
    if (queue.send("torn-good").is_err())
        return 4;

    auto segment = ca::process::ipc::SharedMemory::open(name);
    if (segment.is_err())
        return 5;
    auto  memory = std::move(segment).unwrap();
    auto* header = static_cast<ca::process::ipc::detail::RingHeader*>(memory.data());
    const ca::u64 next_sequence = header->write_seq.load();
    auto* slot = reinterpret_cast<ca::process::ipc::detail::RingSlotHeader*>(
        static_cast<char*>(memory.data()) + ca::process::ipc::detail::kRingHeaderSize +
        (next_sequence % header->slot_count) * header->slot_stride);
    slot->state.store(static_cast<ca::u32>(ca::process::ipc::detail::RingSlotState::Writing));
    slot->length.store(8);
    slot->sequence.store(0xDEADBEEF);
    std::memcpy(reinterpret_cast<char*>(slot) + sizeof(ca::process::ipc::detail::RingSlotHeader),
                "TORNATE",
                8);
    // 不走析构（等价于进程崩溃）：写者身份与撕裂槽一并留在共享内存里。
    std::_Exit(9);
}

// 读者角色：循环接收并逐行输出，写者死亡排空后打印 RECEIVED=<n>；
// 20 秒仍收不齐则以退出码 7 失败（防止父进程测试死等）。
static int ring_reader_child(const char* name, const char* expect_text)
{
    auto opened = ca::process::ipc::ShmRingQueue::open(name);
    if (opened.is_err())
        return 3;
    auto  queue  = std::move(opened).unwrap();
    const int expect_count = std::atoi(expect_text);
    int   received = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    for (;;) {
        auto pending = queue.receive_for(std::chrono::milliseconds(50));
        if (pending.is_err()) {
            if (pending.unwrap_err().code() == ca::core::StatusCode::UNAVAILABLE)
                break;
            return 5;
        }
        auto message = std::move(pending).unwrap();
        if (message.has_value()) {
            std::cout << *message << "\n";
            ++received;
        }
        else if (std::chrono::steady_clock::now() > deadline) {
            return 7;
        }
    }
    std::cout << "RECEIVED=" << received << std::flush;
    return received == expect_count ? 0 : 6;
}

int main(int argc, char** argv)
{
    if (argc == 4 && std::strcmp(argv[1], "--ipc-ring-writer") == 0)
        return ring_writer_child(argv[2], argv[3]);
    if (argc == 5 && std::strcmp(argv[1], "--ipc-ring-writer-hang") == 0)
        return ring_writer_hang_child(argv[2], argv[3], argv[4]);
    if (argc == 3 && std::strcmp(argv[1], "--ipc-ring-writer-torn") == 0)
        return ring_writer_torn_child(argv[2]);
    if (argc == 4 && std::strcmp(argv[1], "--ipc-ring-reader") == 0)
        return ring_reader_child(argv[2], argv[3]);

    if (argc == 2 && std::strcmp(argv[1], "--subprocess-success") == 0) {
        std::cout << "stdout";
        std::cerr << "stderr";
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-timeout") == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-partial-output") == 0) {
        std::cout << "partial" << std::flush;
        std::cerr << "oops" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-failure") == 0) {
        return 7;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-echo") == 0) {
        std::string input;
        std::getline(std::cin, input);
        std::cout << input;
        return 0;
    }
    if (argc == 4 && std::strcmp(argv[1], "--subprocess-args") == 0) {
        std::cout << argv[2] << "|" << argv[3];
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-env") == 0) {
        const char* path     = std::getenv("PATH");
        const char* override = std::getenv("LIBCA_PROCESS_OVERRIDE");
        std::cout << (path != nullptr && *path != '\0' ? "inherited" : "missing") << "|"
                  << (override == nullptr ? "" : override);
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--subprocess-large-output") == 0) {
        const std::string bytes(256 * 1024, 'x');
        std::cout << bytes;
        std::cerr << bytes;
        return 0;
    }

    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
