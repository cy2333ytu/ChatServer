#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>

namespace ccy {

class Redis {
public:
    Redis();
    ~Redis();

    // Connect to the Redis server
    bool connect();

    // Publish a message to the specified channel in Redis
    bool publish(int channel, string message);

    // Subscribe to messages on the specified channel in Redis
    bool subscribe(int channel);

    // Unsubscribe from messages on the specified channel in Redis
    bool unsubscribe(int channel);

    // Receive messages from the subscribed channel in a separate thread
    void observer_channel_message();

    // Initialize the callback object to notify the business layer about channel messages
    void init_notify_handler(std::function<void(int, string)> fn);

private:
    // hiredis synchronous context object for publishing messages
    redisContext *_publish_context;

    // hiredis synchronous context object for subscribing to messages
    redisContext *_subscribe_context;

    // Callback operation to report subscribed messages to the service layer
    std::function<void(int, string)> _notify_message_handler;
};

} // namespace ccy

#endif
