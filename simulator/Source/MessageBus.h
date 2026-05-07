// MessageBus.h
// Simple in-process pub/sub for UI components to communicate.
#pragma once
#include "../../Shared/SharedMessages.h"
#include <functional>
#include <vector>

namespace Sim {

class MessageBus
{
public:
    using Callback = std::function<void(const Message&)>;

    void subscribe(Callback cb)
    {
        subscribers.push_back(std::move(cb));
    }

    void publish(const Message& msg)
    {
        for (auto& cb : subscribers)
            cb(msg);
    }

private:
    std::vector<Callback> subscribers;
};

} // namespace Sim
