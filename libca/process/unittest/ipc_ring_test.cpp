#include <gmock/gmock.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "libca/process/ipc.hpp"
#include "libca/process/subprocess.hpp"

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <limits.h>
#    include <unistd.h>
#endif

namespace ca::process::test {
namespace {

std::string test_executable_path()
{
#if defined(_WIN32)
    char        path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
    return length == 0 || length == sizeof(path) ? std::string{} : std::string(path, length);
#else
    char          path[PATH_MAX]{};
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path));
    return length <= 0 || length == static_cast<ssize_t>(sizeof(path))
               ? std::string{}
               : std::string(path, static_cast<usize>(length));
#endif
}

Command child_command(std::vector<std::string> arguments)
{
    Command command(test_executable_path());
    command.args(std::move(arguments));
    return command;
}

u64 current_process_id()
{
#if defined(_WIN32)
    return static_cast<u64>(GetCurrentProcessId());
#else
    return static_cast<u64>(getpid());
#endif
}

// POSIX 上共享内存名字残留 /dev/shm：声明在最前，保证析构晚于队列对象。
struct ShmNameGuard
{
    explicit ShmNameGuard(std::string value) : name(std::move(value)) {}
    ~ShmNameGuard()
    {
        ipc::remove_shared_memory(name);
    }
    std::string name;
};

// 打开队列底层共享内存段，返回裸头部指针（生命周期跟随 memory）。
ipc::detail::RingHeader* open_ring_header(const std::string& name, ipc::SharedMemory& memory)
{
    auto opened = ipc::SharedMemory::open(name);
    EXPECT_TRUE(opened.is_ok()) << (opened.is_err() ? opened.unwrap_err().to_string() : "");
    if (opened.is_err())
        return nullptr;
    memory = std::move(opened).unwrap();
    return static_cast<ipc::detail::RingHeader*>(memory.data());
}

// 取一个确定已退出（已回收）的进程 pid，供「进程不在了」的探活测试使用。
u64 reaped_child_pid()
{
    auto spawned = child_command({"--subprocess-success"}).spawn();
    EXPECT_TRUE(spawned.is_ok()) << (spawned.is_err() ? spawned.unwrap_err().to_string() : "");
    if (spawned.is_err())
        return 0;
    auto        child = std::move(spawned).unwrap();
    const u64   pid   = child.id();
    const auto  status = child.wait();
    EXPECT_TRUE(status.is_ok());
    return pid;
}

// 子进程经管道回传的 stdout 在 Windows 上是文本模式（\n → \r\n），断言前归一化。
std::string normalize_crlf(std::string text)
{
    usize position = 0;
    while ((position = text.find("\r\n", position)) != std::string::npos)
        text.erase(position, 1);
    return text;
}

// 把头部写者身份改成一个已退出进程 + 心跳归零：同时满足「心跳超时」与「进程不在了」。
void poke_dead_writer(ipc::detail::RingHeader* header, u64 dead_pid)
{
    header->writer_slot.store(dead_pid);
    header->writer_birth.store(0, std::memory_order_relaxed);
    header->writer_heartbeat.store(0, std::memory_order_relaxed);
}

// 直接构造撕裂槽：在 write_seq 指向的槽里留下 Writing 态 + 半截 payload。
void poke_torn_slot(ipc::SharedMemory& memory, ipc::detail::RingHeader* header)
{
    const u64 sequence = header->write_seq.load();
    auto*     slot = reinterpret_cast<ipc::detail::RingSlotHeader*>(static_cast<char*>(
                     memory.data()) + ipc::detail::kRingHeaderSize +
                 (sequence % header->slot_count) * header->slot_stride);
    slot->state.store(static_cast<u32>(ipc::detail::RingSlotState::Writing));
    slot->length.store(8);
    slot->sequence.store(0xDEADBEEF);
    std::memcpy(reinterpret_cast<char*>(slot) + sizeof(ipc::detail::RingSlotHeader),
                "TORNATE",
                8);
}

// 轮询接管（接管门槛含心跳超时，需要等心跳落后）。
bool poll_reset_if_writer_dead(ipc::ShmRingQueue& queue)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        auto result = queue.reset_if_writer_dead();
        EXPECT_TRUE(result.is_ok()) << (result.is_err() ? result.unwrap_err().to_string() : "");
        if (result.is_err())
            return false;
        if (result.unwrap())
            return true;
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// ---- 基础收发与参数 ----

TEST(ShmRingQueueTest, RoundTripWithinSingleProcess)
{
    const std::string name  = "libca_ring_rt_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);

    ipc::ShmRingQueue::Options options{};
    options.max_message_size = 64;
    options.slot_count       = 8;
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto writer = std::move(created).unwrap();

    ASSERT_TRUE(writer.send("first").is_ok());
    ASSERT_TRUE(writer.send("second").is_ok());
    ASSERT_TRUE(writer.send(std::string("third")).is_ok());

    auto reader = ipc::ShmRingQueue::open(name);
    ASSERT_TRUE(reader.is_ok()) << reader.unwrap_err().to_string();
    auto reader_value = std::move(reader).unwrap();

    const char* expected[] = {"first", "second", "third"};
    for (const char* message : expected) {
        auto received = reader_value.receive_for(std::chrono::milliseconds(200));
        ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
        ASSERT_TRUE(received.unwrap().has_value());
        EXPECT_EQ(*received.unwrap(), message);
    }
    // 写者（本进程）仍存活：超时返回空 optional 而非 UNAVAILABLE。
    auto drained = reader_value.receive_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(drained.is_ok()) << drained.unwrap_err().to_string();
    EXPECT_FALSE(drained.unwrap().has_value());
    EXPECT_TRUE(writer.is_writer_alive());
}

TEST(ShmRingQueueTest, BlockingReceiveEndsWhenWriterDetaches)
{
    const std::string name  = "libca_ring_blk_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    auto created = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();
    auto writer = std::move(created).unwrap();
    // Windows 命名映射随最后一个句柄消失：读者必须先打开再让写者退出。
    auto reader = ipc::ShmRingQueue::open(name);
    ASSERT_TRUE(reader.is_ok()) << reader.unwrap_err().to_string();
    auto reader_value = std::move(reader).unwrap();
    ASSERT_TRUE(writer.send("only").is_ok());
    writer.close();   // 优雅释放写者身份

    auto received = reader_value.receive();
    ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
    EXPECT_EQ(received.unwrap(), "only");
    // 写者优雅退出后身份已释放：再次 receive 等待新写者，用短时限确认不误报死亡。
    auto idle = reader_value.receive_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(idle.is_ok()) << idle.unwrap_err().to_string();
    EXPECT_FALSE(idle.unwrap().has_value());
}

TEST(ShmRingQueueTest, CreateTwiceYieldsAlreadyExists)
{
    const std::string name  = "libca_ring_dup_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    auto first = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(first.is_ok()) << first.unwrap_err().to_string();
    auto second = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(second.is_err());
    EXPECT_EQ(second.unwrap_err().code(), ca::core::StatusCode::ALREADY_EXISTS);
}

TEST(ShmRingQueueTest, OpenMissingQueueYieldsNotFound)
{
    auto opened = ipc::ShmRingQueue::open("libca_ring_missing_" + std::to_string(current_process_id()));
    ASSERT_TRUE(opened.is_err());
    EXPECT_EQ(opened.unwrap_err().code(), ca::core::StatusCode::NOT_FOUND);
}

TEST(ShmRingQueueTest, RejectsInvalidOptions)
{
    ipc::ShmRingQueue::Options bad_size{};
    bad_size.max_message_size = 0;
    auto size_result = ipc::ShmRingQueue::create("libca_ring_bad", bad_size);
    EXPECT_EQ(size_result.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);

    ipc::ShmRingQueue::Options bad_slots{};
    bad_slots.slot_count = 0;
    auto slots_result = ipc::ShmRingQueue::create("libca_ring_bad", bad_slots);
    EXPECT_EQ(slots_result.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);

    ipc::ShmRingQueue::Options bad_timeout{};
    bad_timeout.heartbeat_timeout = std::chrono::milliseconds(0);
    auto timeout_result = ipc::ShmRingQueue::create("libca_ring_bad", bad_timeout);
    EXPECT_EQ(timeout_result.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);

    ipc::ShmRingQueue::Options oversized{};
    oversized.slot_count       = 65536;
    oversized.max_message_size = usize{32} << 20;   // 组合超过 1 GiB 段上限
    auto combined_result = ipc::ShmRingQueue::create("libca_ring_bad", oversized);
    EXPECT_EQ(combined_result.unwrap_err().code(), ca::core::StatusCode::INVALID_ARGUMENT);
}

TEST(ShmRingQueueTest, RejectsOversizedMessage)
{
    const std::string name  = "libca_ring_ovs_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    ipc::ShmRingQueue::Options options{};
    options.max_message_size = 8;
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok());
    auto queue  = std::move(created).unwrap();
    auto result = queue.send(std::string("0123456789"));
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.code(), ca::core::StatusCode::OUT_OF_RANGE);
}

TEST(ShmRingQueueTest, SendRefreshesHeartbeatAndCloseReleasesClaim)
{
    const std::string name  = "libca_ring_hb_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    auto created = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(created.is_ok());
    auto queue = std::move(created).unwrap();
    // 独立的裸映射：队列对象 close 后仍可检查头部（Windows 命名映射随最后一个句柄消失）。
    ipc::SharedMemory memory;
    auto* header = open_ring_header(name, memory);
    ASSERT_NE(header, nullptr);
    EXPECT_EQ(header->writer_heartbeat.load(), static_cast<u64>(0));   // 尚未认领

    ASSERT_TRUE(queue.send("pulse").is_ok());
    EXPECT_NE(header->writer_heartbeat.load(), static_cast<u64>(0));   // send 顺带刷新

    queue.close();
    EXPECT_EQ(header->writer_slot.load(), static_cast<u64>(ipc::detail::kRingWriterFree));
}

// ---- 单进程直接构造内存布局的容错测试 ----

// 接管门槛 = 心跳超时 且 进程不在了，二者缺一不可。
TEST(ShmRingQueueTest, ResetRequiresStaleHeartbeatAndDeadProcess)
{
    const std::string name  = "libca_ring_gate_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    auto created = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(created.is_ok());
    auto queue = std::move(created).unwrap();
    ASSERT_TRUE(queue.send("mine").is_ok());   // 本进程认领写者身份

    {
        ipc::SharedMemory memory;
        auto* header = open_ring_header(name, memory);
        ASSERT_NE(header, nullptr);
        header->writer_heartbeat.store(0, std::memory_order_relaxed);   // 心跳超时
    }
    // 心跳超时但写者进程（本进程）还活着：绝不允许接管。
    auto guarded = queue.reset_if_writer_dead();
    ASSERT_TRUE(guarded.is_ok()) << guarded.unwrap_err().to_string();
    EXPECT_FALSE(guarded.unwrap());

    // 进程不在了（已回收的子进程 pid）+ 心跳超时：接管成功且世代 +1。
    const u64 dead = reaped_child_pid();
    ASSERT_NE(dead, static_cast<u64>(0));
    ipc::SharedMemory memory;
    auto* header = open_ring_header(name, memory);
    ASSERT_NE(header, nullptr);
    const u64 generation_before = header->generation.load();
    poke_dead_writer(header, dead);

    auto takeover = queue.reset_if_writer_dead();
    ASSERT_TRUE(takeover.is_ok()) << takeover.unwrap_err().to_string();
    EXPECT_TRUE(takeover.unwrap());
    EXPECT_EQ(header->generation.load(), generation_before + 1);
    EXPECT_EQ(header->write_seq.load(), static_cast<u64>(0));   // 环形槽已重置
}

// 世代不匹配：队列被（其它进程）接管重置后，旧读者显式报错而非静默重同步。
TEST(ShmRingQueueTest, GenerationMismatchErrorsAfterExternalReset)
{
    const std::string name  = "libca_ring_gen_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    auto created = ipc::ShmRingQueue::create(name);
    ASSERT_TRUE(created.is_ok());
    auto old_reader = std::move(created).unwrap();
    // 触发读者接入（记录世代与游标）。
    auto initial = old_reader.receive_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(initial.is_ok());
    EXPECT_FALSE(initial.unwrap().has_value());

    // 模拟另一进程接管：直接抬世代号。
    ipc::SharedMemory memory;
    auto* header = open_ring_header(name, memory);
    ASSERT_NE(header, nullptr);
    header->generation.fetch_add(1);

    auto stale = old_reader.receive_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(stale.is_err());
    EXPECT_EQ(stale.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);
}

// 撕裂槽：写者死于写中留下的 Writing 槽必须被跳过，绝不读出撕製数据。
TEST(ShmRingQueueTest, TornSlotSkippedAndNeverRead)
{
    const std::string name  = "libca_ring_torn_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    ipc::ShmRingQueue::Options options{};
    options.max_message_size    = 64;
    options.slot_count          = 4;
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok());
    auto queue = std::move(created).unwrap();
    ASSERT_TRUE(queue.send("first").is_ok());

    ipc::SharedMemory memory;
    auto* header = open_ring_header(name, memory);
    ASSERT_NE(header, nullptr);
    poke_torn_slot(memory, header);
    // 写者身份改成已退出进程：读者立即能判定写者死亡（无需等心跳超时）。
    const u64 dead = reaped_child_pid();
    ASSERT_NE(dead, static_cast<u64>(0));
    poke_dead_writer(header, dead);

    // 先读到崩溃前已提交的消息。
    auto committed = queue.receive_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(committed.is_ok()) << committed.unwrap_err().to_string();
    ASSERT_TRUE(committed.unwrap().has_value());
    EXPECT_EQ(*committed.unwrap(), "first");
    // 撕裂槽被跳过（"TORNATE" 绝不出现），随后给出写者死亡的终止条件。
    auto after_torn = queue.receive_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(after_torn.is_err());
    EXPECT_EQ(after_torn.unwrap_err().code(), ca::core::StatusCode::UNAVAILABLE);

    // 死写者 + 心跳归零：send 的认领路径完成接管重置，新流不受撕裂残留影响。
    auto reborn = queue.send("second");
    ASSERT_TRUE(reborn.is_ok()) << reborn.to_string();
    EXPECT_EQ(header->generation.load(), static_cast<u64>(2));
    auto fresh = queue.receive_for(std::chrono::milliseconds(200));
    ASSERT_TRUE(fresh.is_ok()) << fresh.unwrap_err().to_string();
    ASSERT_TRUE(fresh.unwrap().has_value());
    EXPECT_EQ(*fresh.unwrap(), "second");
}

// ---- 多进程容错（subprocess 自 spawn 测试助手，子角色见 unittest/main.cpp） ----

TEST(ShmRingQueueCrossProcessTest, ReaderChildReceivesAllMessagesInOrder)
{
    const std::string name  = "libca_ring_xp_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    ipc::ShmRingQueue::Options options{};
    options.max_message_size = 64;
    options.slot_count       = 64;
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();

    auto reader_command = child_command({"--ipc-ring-reader", name, "20"});
    reader_command.stdout(Stdio::piped());
    auto reader_spawned = reader_command.spawn();
    ASSERT_TRUE(reader_spawned.is_ok()) << reader_spawned.unwrap_err().to_string();
    auto reader = std::move(reader_spawned).unwrap();

    auto writer_output =
        child_command({"--ipc-ring-writer", name, "20"}).output();
    ASSERT_TRUE(writer_output.is_ok()) << writer_output.unwrap_err().to_string();
    EXPECT_TRUE(writer_output.unwrap().status.success());

    // 先回收写者（POSIX 上 zombie 会让 kill(pid,0) 误判存活），再等读者收尾。
    auto reader_output = reader.wait_with_output();
    ASSERT_TRUE(reader_output.is_ok()) << reader_output.unwrap_err().to_string();
    EXPECT_TRUE(reader_output.unwrap().status.success()) << reader_output.unwrap().stdout_data;

    std::string expect;
    for (int index = 0; index < 20; ++index)
        expect += "msg-" + std::to_string(index) + "\n";
    expect += "RECEIVED=20";
    EXPECT_EQ(normalize_crlf(reader_output.unwrap().stdout_data), expect);
}

TEST(ShmRingQueueCrossProcessTest, TornWriteFromCrashedWriterRecovers)
{
    const std::string name  = "libca_ring_xc_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    ipc::ShmRingQueue::Options options{};
    options.max_message_size    = 64;
    options.slot_count          = 8;
    options.heartbeat_timeout   = std::chrono::milliseconds(50);
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();

    // 子进程发送一条完整消息、手动留下撕裂槽后 _Exit(9)（模拟写者死于写中）。
    auto writer_output = child_command({"--ipc-ring-writer-torn", name}).output();
    ASSERT_TRUE(writer_output.is_ok()) << writer_output.unwrap_err().to_string();
    EXPECT_EQ(writer_output.unwrap().status.code, 9);

    // 两个读者：R1 保持旧世代游标（用于验证接管后报错），R2 负责排空与接管。
    auto old_reader = ipc::ShmRingQueue::open(name, options);
    ASSERT_TRUE(old_reader.is_ok()) << old_reader.unwrap_err().to_string();
    auto r1 = std::move(old_reader).unwrap();
    // 触发 R1 接入（记录世代与游标）。
    auto initial = r1.receive_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(initial.is_ok());

    auto resetter = ipc::ShmRingQueue::open(name, options);
    ASSERT_TRUE(resetter.is_ok()) << resetter.unwrap_err().to_string();
    auto r2 = std::move(resetter).unwrap();

    // R2 排空：先收到崩溃前已提交的消息，然后是写者死亡终止条件（撕裂槽不出现）。
    auto committed = r2.receive();
    ASSERT_TRUE(committed.is_ok()) << committed.unwrap_err().to_string();
    EXPECT_EQ(committed.unwrap(), "torn-good");
    auto drained = r2.receive();
    ASSERT_TRUE(drained.is_err());
    EXPECT_EQ(drained.unwrap_err().code(), ca::core::StatusCode::UNAVAILABLE);

    // R2 读者侧接管重置（心跳超时门槛 50ms，轮询等待）。
    EXPECT_TRUE(poll_reset_if_writer_dead(r2));

    // 旧世代读者 R1 显式报错；接管者 R2 自身则重新接入新流（文档化语义）。
    auto stale = r1.receive_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(stale.is_err());
    EXPECT_EQ(stale.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);

    // 新写者从干净状态认领并发送。
    auto writer = ipc::ShmRingQueue::open(name, options);
    ASSERT_TRUE(writer.is_ok()) << writer.unwrap_err().to_string();
    auto writer_value = std::move(writer).unwrap();
    auto reborn       = writer_value.send("reborn");
    ASSERT_TRUE(reborn.is_ok()) << reborn.to_string();

    // 接管者 R2 已重新接入新流：只看到新消息，撕裂残留绝不出现。
    auto message = r2.receive();
    ASSERT_TRUE(message.is_ok()) << message.unwrap_err().to_string();
    EXPECT_EQ(message.unwrap(), "reborn");
}

TEST(ShmRingQueueCrossProcessTest, KilledWriterExcludesNewWriterThenYields)
{
    const std::string name  = "libca_ring_xk_" + std::to_string(current_process_id());
    ShmNameGuard      guard(name);
    ipc::ShmRingQueue::Options options{};
    options.max_message_size    = 64;
    options.slot_count          = 8;
    options.heartbeat_timeout   = std::chrono::milliseconds(100);
    auto created = ipc::ShmRingQueue::create(name, options);
    ASSERT_TRUE(created.is_ok()) << created.unwrap_err().to_string();

    // 写者子进程发送 3 条后挂住：父进程 kill 模拟崩溃（Windows TerminateProcess / POSIX kill）。
    auto hang_spawned = child_command({"--ipc-ring-writer-hang", name, "3", "30000"}).spawn();
    ASSERT_TRUE(hang_spawned.is_ok()) << hang_spawned.unwrap_err().to_string();
    auto hang = std::move(hang_spawned).unwrap();

    // R1 负责正常消费与后续接管；R2 保持旧世代游标（验证接管后旧读者报错）。
    auto reader = ipc::ShmRingQueue::open(name, options);
    ASSERT_TRUE(reader.is_ok()) << reader.unwrap_err().to_string();
    auto r1 = std::move(reader).unwrap();
    for (int index = 0; index < 3; ++index) {
        auto received = r1.receive_for(std::chrono::seconds(5));
        ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
        ASSERT_TRUE(received.unwrap().has_value());
        EXPECT_EQ(*received.unwrap(), "msg-" + std::to_string(index));
    }

    // 活跃写者（心跳新鲜）排斥其它写者。
    auto rejected = r1.send("intruder");
    ASSERT_TRUE(rejected.is_err());
    EXPECT_EQ(rejected.code(), ca::core::StatusCode::UNAVAILABLE);

    // 崩溃：写者身份还在但进程没了，读者立即获得死亡终止条件。
    ASSERT_TRUE(hang.kill().is_ok());
    auto reaped = hang.wait();
    ASSERT_TRUE(reaped.is_ok()) << reaped.unwrap_err().to_string();
    auto terminal = r1.receive_for(std::chrono::seconds(5));
    ASSERT_TRUE(terminal.is_err());
    EXPECT_EQ(terminal.unwrap_err().code(), ca::core::StatusCode::UNAVAILABLE);

    // R2 在接管前接入旧世代（读取不具破坏性：R2 可能收到保留窗里的旧消息，
    // 也可能直接得到排空终止——无论哪种，世代与游标都已记录）。
    auto bystander = ipc::ShmRingQueue::open(name, options);
    ASSERT_TRUE(bystander.is_ok()) << bystander.unwrap_err().to_string();
    auto r2 = std::move(bystander).unwrap();
    r2.receive_for(std::chrono::milliseconds(1));

    // R1 读者侧接管重置（心跳落后门槛 100ms，轮询等待）。
    EXPECT_TRUE(poll_reset_if_writer_dead(r1));

    // 旧世代读者 R2 显式报错；接管者 R1 重新接入新流。
    auto stale = r2.receive_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(stale.is_err());
    EXPECT_EQ(stale.unwrap_err().code(), ca::core::StatusCode::FAILED_PRECONDITION);

    // 新写者从干净状态继续，接管者 R1 读到新流。
    auto writer_output = child_command({"--ipc-ring-writer", name, "3"}).output();
    ASSERT_TRUE(writer_output.is_ok()) << writer_output.unwrap_err().to_string();
    EXPECT_TRUE(writer_output.unwrap().status.success());
    for (int index = 0; index < 3; ++index) {
        auto received = r1.receive_for(std::chrono::seconds(5));
        ASSERT_TRUE(received.is_ok()) << received.unwrap_err().to_string();
        ASSERT_TRUE(received.unwrap().has_value());
        EXPECT_EQ(*received.unwrap(), "msg-" + std::to_string(index));
    }
}

}   // namespace
}   // namespace ca::process::test
