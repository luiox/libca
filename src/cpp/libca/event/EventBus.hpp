#ifndef LIBCA_EVENT_EVENTBUS_HPP
#define LIBCA_EVENT_EVENTBUS_HPP

#include <functional>
#include "Event.hpp"


class IEventBus
{
public:
    virtual void subscribe(std::function<void()> callback) = 0;
    virtual void post(Event& event)                        = 0;
}


#endif   // !LIBCA_EVENT_EVENTBUS_HPP
