#ifndef __MUTTY_NET_CONTEXT_H__
#define __MUTTY_NET_CONTEXT_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../utils/Object.h"
#include "InetAddress.h"
#include "Socket.h"
namespace mutty {

class Context: public std::enable_shared_from_this<Context> {
public:
    Future<std::weak_ptr<Context>> makeFuture() {
#if __cplusplus > 201402L || __GNUC__
        return mutty::makeFuture(looper, weak_from_this());
#else
        std::weak_ptr<Context> self = shared_from_this();
        return mutty::makeFuture(looper, std::move(self));
#endif
    }

    Context(Looper *looper, const InetAddress &address, Socket &&socket)
        : looper(looper),
          address(address),
          socket(std::move(socket)) {}

// private:
public:
    Looper *looper;
    InetAddress address;
    Socket socket;
    std::exception_ptr exception;
    // Buffer
    // State
    Object any;
};

} // mutty
#endif