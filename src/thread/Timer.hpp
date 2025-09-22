#pragma once

#include "libca/base/Platform.hpp"

namespace ca {

class PassedTimer
{
private:
    u64 startTime_;

public:
    PassedTimer()
        : startTime_(0)
    {}

    void reset() { startTime_ = 0; }

    void start() {}

    void stop() {}

    bool passedMs(u64 ms) { return false; }

    bool passedS(u64 s) { return false; }

    bool passedM(u64 m) { return false; }
};

class CallbackTimer
{
    private:
    
    public:
};
}   // namespace ca