#ifndef __MUTTY_NET_CONNECTOR_H__
#define __MUTTY_NET_CONNECTOR_H__
#include <unistd.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "InetAddress.h"
#include "Socket.h"
// #include "Multiplexer.h"
namespace mutty {

class Connector {
public:
    Future<Connector*> makeFuture() {
        return mutty::makeFuture(_looper, this);
    }

    Future<std::pair<InetAddress, Socket>> connect(const InetAddress /*server*/&address) {
        Socket socket {Socket::INVALID_FD};
        return makeTupleFuture(_looper, address, std::move(socket), 0)
            .poll([](std::tuple<InetAddress, Socket, int> &&info) {
                auto &address = std::get<0>(info);
                auto &socket = std::get<1>(info);
                int &err = std::get<2>(info);
                socket = Socket();
                int ret = socket.connect(address);
                err = ret ? errno : 0;
                switch(err) {
                    case 0:
                        return true;
                    case EINPROGRESS:
                    case EINTR:
                    case EISCONN:
                        // retry if failed
                        // or go on and err = 0
                        err = Connector::testFailed(socket);
                        return !err;
                    case EAGAIN:
                    case EADDRINUSE:
                    case EADDRNOTAVAIL:
                    case ECONNREFUSED:
                    case ENETUNREACH:
                        // fast retry
                        return false;
                    default:
                        // abort, log errno(err) in cancelIf
                        // err = 1;
                        return true;
                }
            })
            .cancelIf([](std::tuple<InetAddress, Socket, int> &&info) {
                auto err = std::get<2>(info);
                // LOG...
                return !!err;
            })
            .then([](std::tuple<InetAddress, Socket, int/*unused*/> &&info) {
                auto &address = std::get<0>(info);
                auto &socket = std::get<1>(info);
                return std::make_pair(address, std::move(socket));
            });
    }

    std::chrono::milliseconds nextRetry(std::chrono::milliseconds current) {
        using namespace std::chrono_literals;
        using namespace std::chrono;
        return std::min(current + current, duration_cast<milliseconds>(10s));
    }

    Connector(Looper *looper)
        : _looper(looper) {}

private:
    static int testFailed(const Socket &socket) {
        sockaddr jojo;
        socklen_t len = sizeof(jojo);
        if(::getpeername(socket.fd(), &jojo, &len)) {
            return errno;
        }
        return 0;
    }

private:
    Looper *_looper;
};

} // mutty
#endif