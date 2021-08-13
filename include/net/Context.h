#ifndef __MUTTY_NET_CONTEXT_H__
#define __MUTTY_NET_CONTEXT_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../utils/Object.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Buffer.h"
namespace mutty {

class Context /*: public std::enable_shared_from_this<Context>*/ {
public:
    // TODO EventState
    enum class NetworkState {
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
    };

//     Future<std::weak_ptr<Context>> makeFuture() {
// #if __cplusplus > 201402L || __GNUC__
//         return mutty::makeFuture(looper, weak_from_this());
// #else
//         std::weak_ptr<Context> self = shared_from_this();
//         return mutty::makeFuture(looper, std::move(self));
// #endif
//     }

    Future<Context*> makeFuture() {
        return mutty::makeFuture(looper, this);
    }

    Context(Looper *looper, const InetAddress &address, Socket &&socket)
        : looper(looper),
          address(address),
          socket(std::move(socket)) {}

    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = default;

// private:
public:
    Looper *looper;
    InetAddress address;
    Socket socket;
    std::exception_ptr exception;
    Buffer input;
    Buffer output;
    Object any;
// private: TODO friend class
    NetworkState state;

};

} // mutty
#endif