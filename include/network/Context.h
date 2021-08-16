#ifndef __FLUENT_NETWORK_CONTEXT_H__
#define __FLUENT_NETWORK_CONTEXT_H__
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../utils/Object.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Buffer.h"
namespace fluent {

class Multiplexer;
class Context /*: public std::enable_shared_from_this<Context>*/ {
// friends
public:
    template <typename, typename, typename> friend class Handler;

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
        return fluent::makeFuture(looper, this);
    }

    int fd() const { return socket.fd(); }

    void send(const void *buf, size_t n) {
        if(_nState != NetworkState::DISCONNECTING || _nState != NetworkState::DISCONNECTED) {
            // TODO add socket.write() and remove exception in context
            int ret = ::write(socket.fd(), buf, n);
            // TODO confirm wirte-complete
            // TODO futureSend, return Future<...>
            if(ret == n) {
                _sendCompleteCounter++;
                return; // fast return
            }
            if(ret < 0) {
                switch(errno) {
                    case EAGAIN:
                    case EINTR:
                        ret = 0;
                    break;
                    default:
                        // Log...
                        // caught by handler
                        throw WriteException(errno);
                        // or no throw?
                        // future is hard to catch exception (try-catch everytime)
                        // TODO add futureSend interface
                    break;
                }
            }
            // ret >= 0
            // TODO submit request to buffer with readyFlag and vector
            output.append(static_cast<const char *>(buf) + ret, n - ret);
            _readyToCompleteCounter++;
            if(!(_events & EVENT_WRITE)) {
                _events |= EVENT_WRITE;
                // TODO remove this ugly code, use handler when any callback
                auto hint = updateEventState();
                updateMultiplexer(hint);
                // will disable write when buf.unread == 0 (handleWrite)
            }
        }
    }

    template <size_t N>
    void send(const char (&buf)[N]) {
        send(buf, N-1); // '\0'
    }

    void send(const std::string &str) {
        send(str.c_str(), str.size());
    }

    // for future.poll
    bool sendCompleteTestAndSet() {
        if(_sendCompleteCounter) {
            _sendCompleteCounter--;
            return true;
        }
        return false;
    }

    bool sendComplete() {
        return _sendCompleteCounter;
    }

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

    // for complete callback and future
    size_t _sendCompleteCounter {0};
    size_t _readyToCompleteCounter {0};

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

    // ugly...
    Multiplexer *_multiplexer;
    std::pair<size_t, Context> *_bundle;

    // ensure: handle init
    // limit: called by send
    void updateMultiplexer(EpollOperationHint hint);

// for multiplexer END
};

} // fluent
#endif