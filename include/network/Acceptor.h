#ifndef __FLUENT_NETWORK_ACCEPTOR_H__
#define __FLUENT_NETWORK_ACCEPTOR_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "InetAddress.h"
#include "Socket.h"
namespace fluent {

class Acceptor /*: public std::enable_shared_from_this<Acceptor>*/ {
public:
    Future<Acceptor*> makeFuture() {
        return fluent::makeFuture(_looper, this);
    }

    void start() {
        _listenDescriptor.listen();
    }

    // TODO use optional
    // std::shared_ptr<Context> accept() {
    //     ...
    // }

    bool accept() {
        socklen_t len = sizeof(_lastBufferedAddress);
        int maybeFd = ::accept4(_listenDescriptor.fd(), (sockaddr*)(&_lastBufferedAddress), &len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(maybeFd >= 0) {
            _lastBufferedSocket = Socket(maybeFd);
            return true;
        }
        // LOG...
        return false;
    }

    // ensure: accept
    // can only get once per request
    std::pair<InetAddress, Socket> aceeptResult() {
        return std::make_pair(_lastBufferedAddress, std::move(_lastBufferedSocket));
    }

    Acceptor(Looper *looper, InetAddress address)
        : _looper(looper),
          _address(address) {
        _listenDescriptor.setReusePort();
        _listenDescriptor.setReuseAddr();
        _listenDescriptor.bind(address);
    }

    Acceptor(const Acceptor&) = delete;
    Acceptor(Acceptor&&) = default;
    Acceptor& operator=(const Acceptor&) = delete;
    Acceptor& operator=(Acceptor&&) = default;


private:
    Looper *_looper; // TODO remove looper
    InetAddress _address;
    Socket _listenDescriptor;

    // buffered by accept
    InetAddress _lastBufferedAddress;
    // TODO use single union idiom to avoid construct
    // union { Socket buffered }
    Socket _lastBufferedSocket {Socket::INVALID_FD};
};

} // fluent
#endif