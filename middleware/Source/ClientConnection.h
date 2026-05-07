// ClientConnection.h
// Represents one connected client (TCP or serial).
// Owns a FrameParser for inbound data and a send queue for outbound data.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/Protocol.h"
#include <functional>
#include <mutex>
#include <deque>

class ClientConnection
{
public:
    using MessageCallback = std::function<void(const Sim::Message&)>;

    explicit ClientConnection(MessageCallback cb)
        : parser([this, cb](const Sim::Message& msg) { cb(msg); }) {}

    virtual ~ClientConnection() = default;

    /** Send a message to this client. Thread-safe. */
    virtual void send(const Sim::Message& msg) = 0;

    /** Returns false when the connection is no longer alive. */
    virtual bool isConnected() const = 0;

protected:
    Protocol::FrameParser parser;
};
