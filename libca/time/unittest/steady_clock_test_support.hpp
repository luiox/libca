#pragma once

#include <chrono>
#include <thread>

namespace ca::time::test {

inline void wait_for_elapsed(std::chrono::steady_clock::duration delay)
{
    // Windows 的短休眠可能提前返回，用单调时钟确认已达到所需时长。
    const auto deadline = std::chrono::steady_clock::now() + delay;
    for (auto remaining = delay; remaining > std::chrono::steady_clock::duration::zero();
         remaining = deadline - std::chrono::steady_clock::now()) {
        std::this_thread::sleep_for(std::chrono::ceil<std::chrono::milliseconds>(remaining));
    }
}

}  // namespace ca::time::test
