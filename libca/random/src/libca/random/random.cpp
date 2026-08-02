#include "libca/random/random.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "libca/crypto/random.hpp"

namespace ca::random {

namespace {

constexpr char kHexDigits[]      = "0123456789abcdef";
constexpr char kAlphanumeric[]   = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr usize kAlphanumericLen = 62;  // 10 数字 + 26 小写 + 26 大写

// 从系统 CSPRNG 取一块原始字节。失败抛异常。
void read_entropy(void* buf, usize len)
{
    if (len == 0)
        return;
    auto result = ca::crypto::secure_random_bytes(len);
    if (result.is_err())
        throw std::runtime_error("ca::random: secure random source failed");
    auto bytes = result.unwrap();
    if (bytes.len() < len)
        throw std::runtime_error("ca::random: insufficient random bytes");
    std::memcpy(buf, bytes.as_ptr(), len);
}

// 批量熵缓冲区，减少每次取随机数都发起系统调用的开销。
// 一次补充一批 u64，消费完再补充。线程局部存储保证线程安全且无锁竞争。
class EntropyPool
{
public:
    u64 next_u64()
    {
        if (pos_ >= kPoolSize)
            refill();
        return pool_[pos_++];
    }

private:
    void refill()
    {
        read_entropy(pool_, sizeof(pool_));
        pos_ = 0;
    }

    static constexpr usize kPoolSize = 32;  // 32 * 8 = 256 字节/次系统调用
    u64   pool_[kPoolSize];
    usize pos_{kPoolSize};  // 初始为空，首次 next_u64 触发 refill
};

EntropyPool& thread_pool()
{
    thread_local EntropyPool pool;
    return pool;
}

}  // namespace

void fill_bytes(void* buf, usize len)
{
    read_entropy(buf, len);
}

u64 next(u64 n)
{
    assert(n > 0 && "ca::random::next: n must be > 0");
    if (n == 0)
        throw std::invalid_argument("ca::random::next: n must be > 0");

    // 拒绝采样：把 [0, 2^64) 截断到最大不超过 n 的整数倍区间，超出则重取。
    // 用 UINT64_MAX 计算，避免 2^64 溢出回绕成 0 导致死循环（n 为 2 的幂时）：
    //   limit = (2^64 - 1) - ((2^64 - 1) - n + 1) % n
    // 当 n 整除 2^64 时，((2^64-n)) % n == 0，limit = UINT64_MAX，全部接受。
    const u64 limit = UINT64_MAX - (UINT64_MAX - n + 1) % n;
    auto&    pool   = thread_pool();
    u64      value;
    do {
        value = pool.next_u64();
    } while (value > limit);
    return value % n;
}

u64 range(u64 lo, u64 hi)
{
    assert(hi > lo && "ca::random::range: hi must be > lo");
    if (hi <= lo)
        throw std::invalid_argument("ca::random::range: hi must be > lo");
    return lo + next(hi - lo);
}

double probability()
{
    // 取 53 位作为 double 尾数，除以 2^53 得到 [0, 1)。
    constexpr u64 kMask = (static_cast<u64>(1) << 53) - 1;
    constexpr double kScale = static_cast<double>(static_cast<u64>(1) << 53);
    return static_cast<double>(thread_pool().next_u64() & kMask) / kScale;
}

std::string hex_string(usize len)
{
    std::string out;
    out.resize(len * 2);
    std::string raw;
    raw.resize(len);
    read_entropy(raw.data(), len);
    for (usize i = 0; i < len; ++i) {
        auto byte = static_cast<unsigned char>(raw[i]);
        out[i * 2]     = kHexDigits[byte >> 4];
        out[i * 2 + 1] = kHexDigits[byte & 0x0F];
    }
    return out;
}

std::string alphanumeric_string(usize len)
{
    std::string out;
    out.resize(len);
    for (usize i = 0; i < len; ++i) {
        // next() 内部用 thread_local 熵池，单字符开销已很低。
        out[i] = kAlphanumeric[next(kAlphanumericLen)];
    }
    return out;
}

}  // namespace ca::random
