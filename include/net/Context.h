#ifndef __MUTTY_NET_CONTEXT_H__
#define __MUTTY_NET_CONTEXT_H__
#include <poll.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../utils/Object.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Buffer.h"
namespace mutty {

class Context /*: public std::enable_shared_from_this<Context>*/ {
// friends
public:
    template <typename, typename, typename> friend class Handler;
    friend class ConnectHandler;

// networks
public:
    enum class NetworkState {
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
    };

    bool isConnecting() const { return _nState == NetworkState::CONNECTING; }
    bool isConnected() const { return _nState == NetworkState::CONNECTED; }
    bool isDisConnecting() const { return _nState == NetworkState::DISCONNECTING; }
    bool isDisConnected() const { return _nState == NetworkState::DISCONNECTED; }

    // A call shutdown and set DISCONNECTING ---> send FIN ---> B recv 0 ---> B handleClose ---> B shutdown and set DISCONNECTED
    // B send FIN ---> A recv 0 ---> A handleClose ---> A DISCONNECTED
    // if failed, A keeps DISCONNECTING? (B-side context not in epoll? crashed?)
    void shutdown() {
        if(_nState == NetworkState::CONNECTED) {
            // dont care peer, DISCONNECTING can still handleClose
            _nState = NetworkState::DISCONNECTING;
            // A can send FIN
            // inner write flag will be turned off when handleClose
            // but socket shutdown-WR inplace
            // poll trigged -> buffer.writeTo -> ...
            if(_events & EVENT_WRITE) {
                socket.shutdown();
            }
        }
    }

public:
    Future<Context*> makeFuture() {
        return mutty::makeFuture(looper, this);
    }

    int fd() const { return socket.fd(); }

    Context(Looper *looper, const InetAddress &address, Socket &&socket)
        : looper(looper),
          address(address),
          socket(std::move(socket)) {}

    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = default;

// shared context
public:
    Looper *looper;
    InetAddress address;
    Socket socket;
    // captured exception
    std::exception_ptr exception;
    Buffer input;
    Buffer output;
    // user context, anything is ok
    Object any;

// modified by friend
private:
    NetworkState _nState {NetworkState::CONNECTING};

// for multiplexer START
// TODO class
public:
    using EventBitmap = uint32_t;
    constexpr static EventBitmap EVENT_NONE = 0;
    constexpr static EventBitmap EVENT_READ = POLL_IN | POLL_PRI;
    constexpr static EventBitmap EVENT_WRITE = POLL_OUT;

    enum class EventState {
        NEW,
        ADDED,
        DELETED
    };

    // TODO friend class
    using EpollOperationHint = uint;
    constexpr static EpollOperationHint EPOLL_CTL_NONE = 0;

    // update the event state and return next operation hint for epoll
    EpollOperationHint updateEventState() {
        if(_eState == EventState::NEW || _eState == EventState::DELETED) {
            _eState = EventState::ADDED;
            return EPOLL_CTL_ADD;
        }

        // else : EventState::ADD

        if(_events == EVENT_NONE) {
            _eState = EventState::DELETED;
            return EPOLL_CTL_DEL;
        } else {
            return EPOLL_CTL_MOD;
        }
    }

    EventBitmap events() const { return _events; }
    bool readEventEnabled() const { return _events & EVENT_READ; }
    bool writeEventEnabled() const { return _events & EVENT_WRITE; }

    // note: must update state for epoll
    bool enableRead() {
        if(!(_events & EVENT_READ)) {
            _events |= EVENT_READ;
            return true;
        }
        return false;
    }

    bool enableWrite() {
        if(!(_events & EVENT_WRITE)) {
            _events |= EVENT_WRITE;
            return true;
        }
        return false;
    }

    bool disableRead() {
        if(_events & EVENT_READ) {
            _events &= ~EVENT_READ;
            return true;
        }
        return false;
    }

    bool disableWrite() {
        if(_events & EVENT_WRITE) {
            _events &= ~EVENT_WRITE;
            return true;
        }
        return false;
    }

private:
    EventBitmap _events {EVENT_NONE};
    EventState _eState {EventState::NEW};

    constexpr static void checkHardCode() {
        static_assert(EPOLL_CTL_NONE != EPOLL_CTL_ADD
                      && EPOLL_CTL_NONE != EPOLL_CTL_DEL
                      && EPOLL_CTL_NONE != EPOLL_CTL_MOD,
                      "check EPOLL_CTL_NONE");
    }

// for multiplexer END
};

} // mutty
#endif