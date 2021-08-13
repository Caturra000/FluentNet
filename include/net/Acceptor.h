#ifndef __MUTTY_NET_ACCEPTOR_H__
#define __MUTTY_NET_ACCEPTOR_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Context.h"
namespace mutty {

class Acceptor /*: public std::enable_shared_from_this<Acceptor>*/ {
public:
    // Future<std::shared_ptr<Acceptor>> makeFutureGuarded() {
    //     return mutty::makeFuture(_looper, shared_from_this());
    // }

    Future<Acceptor*> makeFuture() {
        return mutty::makeFuture(_looper, this);
    }

    void start() {
        _listenDescriptor.listen();
    }

    std::shared_ptr<Context> accept() {
        // stack alloc
        InetAddress contextAddress;
        socklen_t len = sizeof(contextAddress);
        int maybeFd = ::accept4(_listenDescriptor.fd(), (sockaddr*)(&contextAddress), &len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(maybeFd >= 0) {
            return std::make_shared<Context>(_looper, contextAddress, Socket(maybeFd));
        }
        // no throw even if not EAGAIN
        return nullptr;
    }

    Acceptor(Looper *looper, InetAddress address)
        : _looper(looper),
          _address(address),
          _listenDescriptor() {
        _listenDescriptor.setReusePort();
        _listenDescriptor.setReuseAddr();
        _listenDescriptor.bind(address);
    }
    // Acceptor(Acceptor&&rhs)
    //     : _looper(rhs._looper),
    //       _address(rhs._address),
    //       _listenDescriptor(std::move(rhs._listenDescriptor)) {}

    Acceptor(const Acceptor&) = delete;
    Acceptor(Acceptor&&) = default;
    Acceptor& operator=(const Acceptor&) = delete;
    Acceptor& operator=(Acceptor&&) = default;


private:
    Looper *_looper;
    InetAddress _address;
    Socket _listenDescriptor;
};

} // mutty
#endif