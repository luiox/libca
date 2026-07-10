#include "libca/thread/thread.hpp"

#include <mutex>

namespace ca::thread {
namespace details {

class ThreadCompletion
{
public:
    std::mutex         mutex;
    std::exception_ptr exception;
};

std::shared_ptr<ThreadCompletion> make_thread_completion()
{
    return std::make_shared<ThreadCompletion>();
}

void record_thread_exception(const std::shared_ptr<ThreadCompletion>& completion,
                             std::exception_ptr exception) noexcept
{
    try {
        std::lock_guard<std::mutex> lock(completion->mutex);
        completion->exception = std::move(exception);
    }
    catch (...) {
    }
}

ca::core::Status completion_status(const std::shared_ptr<ThreadCompletion>& completion)
{
    std::exception_ptr exception;
    {
        std::lock_guard<std::mutex> lock(completion->mutex);
        exception = completion->exception;
    }
    if (exception == nullptr)
        return ca::core::OkStatus();

    try {
        std::rethrow_exception(exception);
    }
    catch (const std::exception& error) {
        return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                   std::string("thread function threw: ") + error.what());
    }
    catch (...) {
        return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                   "thread function threw a non-standard exception");
    }
}

}   // namespace details

Thread::Thread(std::thread native,
               StopSource stop_source,
               std::shared_ptr<details::ThreadCompletion> completion) noexcept
    : native_(std::move(native))
    , stop_source_(std::move(stop_source))
    , completion_(std::move(completion))
    , started_(true)
{}

Thread::~Thread()
{
    finish_noexcept();
}

Thread::Thread(Thread&& other) noexcept
    : native_(std::move(other.native_))
    , stop_source_(std::move(other.stop_source_))
    , completion_(std::move(other.completion_))
    , started_(other.started_)
    , joined_(other.joined_)
    , join_status_(std::move(other.join_status_))
{
    other.started_ = false;
    other.joined_  = false;
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this != &other) {
        finish_noexcept();
        native_        = std::move(other.native_);
        stop_source_   = std::move(other.stop_source_);
        completion_    = std::move(other.completion_);
        started_       = other.started_;
        joined_        = other.joined_;
        join_status_   = std::move(other.join_status_);
        other.started_ = false;
        other.joined_  = false;
    }
    return *this;
}

bool Thread::joinable() const noexcept
{
    return native_.joinable();
}

std::thread::id Thread::id() const noexcept
{
    return native_.get_id();
}

StopToken Thread::stop_token() const noexcept
{
    return stop_source_.has_value() ? stop_source_->token() : StopToken{};
}

bool Thread::request_stop() noexcept
{
    return stop_source_.has_value() && stop_source_->request_stop();
}

ca::core::Status Thread::join()
{
    if (!started_)
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "join on an empty Thread");
    if (joined_)
        return join_status_;
    if (!native_.joinable())
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "thread is not joinable");
    if (native_.get_id() == std::this_thread::get_id())
        return ca::core::ErrStatus(ca::core::StatusCode::FAILED_PRECONDITION,
                                   "a thread cannot join itself");

    try {
        native_.join();
    }
    catch (const std::system_error& error) {
        return ca::core::ErrStatus(ca::core::StatusCode::INTERNAL,
                                   std::string("thread join failed: ") + error.what());
    }

    joined_      = true;
    join_status_ = details::completion_status(completion_);
    return join_status_;
}

void Thread::finish_noexcept() noexcept
{
    if (!native_.joinable())
        return;

    request_stop();
    if (native_.get_id() == std::this_thread::get_id()) {
        native_.detach();
        return;
    }
    try {
        native_.join();
    }
    catch (...) {
        try {
            native_.detach();
        }
        catch (...) {
        }
    }
}

}   // namespace ca::thread
