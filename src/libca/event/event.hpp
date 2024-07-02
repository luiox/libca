#ifndef LIBCA_EVENT_EVENT_H
#define LIBCA_EVENT_EVENT_H

#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace libca::event {

// 事件优先级
enum class EventPriority
{
    Lowest  = std::numeric_limits<int>::lowest(),
    Low     = 0,
    Medium  = 1000,
    High    = 2000,
    Highest = std::numeric_limits<int>::max()
};

// 事件
class Event
{
public:
    Event(const std::string& name, EventPriority priority = EventPriority::Medium);
    virtual ~Event();

    const std::string& GetName() const;
    EventPriority      GetPriority() const;

    virtual void Trigger() = 0;

protected:
    std::string   name;
    EventPriority priority;
};

// 事件监听器
class EventListner
{
public:
};

// 事件分发器
class EventDispatcher
{};

// 事件管理器
class EventManager
{
public:
    EventManager();
    ~EventManager();
};

}   // namespace libca::event

#endif   // !LIBCA_EVENT_EVENT_H
