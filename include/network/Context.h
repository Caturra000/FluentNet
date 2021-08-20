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
    void shutdown();

// networks -modified by friends
private:
    NetworkState _nState {NetworkState::CONNECTING};

// common
public:
    Future<Context*> makeFuture() { return fluent::makeFuture(looper, this); }

    int fd() const { return socket.fd(); }

// send
public:
    class Completion {
    public:
        // index flag: never
        constexpr static size_t INVALID = 0;
        // index flag: always
        constexpr static size_t FAST_COMPLETE = 1;
    private:
        size_t _token;
        Context *_master;
    public:
        bool poll() const { return _master->_sendCompletedIndex > _token; }
        Completion(size_t token, Context *master)
            : _token(token), _master(master) {}
    };

    Completion send(const void *buf, size_t n);

    template <size_t N>
    Completion send(const char (&buf)[N]) {  return send(buf, N-1); /*'\0'*/ }

    Completion send(const std::string &str) { return send(str.c_str(), str.size()); }

// send -modified by friends
private:
    std::queue<size_t> _sendProvider;
    // 0 - must be completed
    size_t _sendCurrentIndex {Completion::FAST_COMPLETE + 1};
    size_t _sendCompletedIndex {Completion::FAST_COMPLETE + 1};

// class attribute
public:
    Context(Looper *looper, const InetAddress &address, Socket &&socket)
        : looper(looper),
          address(address),
          socket(std::move(socket)) {}

    ~Context() = default;
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

// for multiplexer
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
    EpollOperationHint updateEventState();

    EventBitmap events() const { return _events; }
    bool readEventEnabled() const { return _events & EVENT_READ; }
    bool writeEventEnabled() const { return _events & EVENT_WRITE; }

    // note: must update state for epoll
    bool enableRead();
    bool enableWrite();
    bool disableRead();
    bool disableWrite();

// for multiplexer
private:
    EventBitmap _events {EVENT_NONE};
    EventState _eState {EventState::NEW};

    constexpr static void checkHardCode();

    // ugly...
    Multiplexer *_multiplexer;
    std::pair<size_t, Context> *_bundle;

    // ensure: handle init
    // limit: called by send
    void updateMultiplexer(EpollOperationHint hint);

// log helper
public:
    // trace
    uint64_t hashcode() const;
    const char* networkInfo() const;
    char eventStateInfo() const;
    char eventsInfo() const;
    const std::string& simpleInfo() const;

// log helper
private:
    mutable uint64_t _hashcode {};
    mutable std::string _cachedInfo;
};

inline void Context::shutdown() {
    if(_nState == NetworkState::CONNECTED) {
        // dont care peer, DISCONNECTING can still handleClose
        _nState = NetworkState::DISCONNECTING;
        // ensure buffer is empty
        // TODO add forceClose
        if(!(_events & EVENT_WRITE)) {
            socket.shutdown();
        }
    }
}

inline Context::Completion Context::send(const void *buf, size_t n) {
    if(_nState != NetworkState::DISCONNECTING && _nState != NetworkState::DISCONNECTED) {
        ssize_t ret = socket.write(buf, n);
        if(ret == n) {
            // fast return
            return Completion{Completion::FAST_COMPLETE, this};
        }
        // TODO submit request to buffer with readyFlag and vector
        output.append(static_cast<const char *>(buf) + ret, n - ret);
        _sendProvider.emplace(n - ret);
        size_t sendToken = _sendCurrentIndex++;
        if(!(_events & EVENT_WRITE)) {
            _events |= EVENT_WRITE;
            // TODO remove this ugly code, use handler when any callback
            auto hint = updateEventState();
            updateMultiplexer(hint);
            // will disable write when buf.unread == 0 (handleWrite)
        }
        return Completion{sendToken, this};
    }
    return Completion{Completion::INVALID, this};
}

inline Context::EpollOperationHint Context::updateEventState() {
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

inline bool Context::enableRead() {
    if(!(_events & EVENT_READ)) {
        _events |= EVENT_READ;
        return true;
    }
    return false;
}

inline bool Context::enableWrite() {
    if(!(_events & EVENT_WRITE)) {
        _events |= EVENT_WRITE;
        return true;
    }
    return false;
}

inline bool Context::disableRead() {
    if(_events & EVENT_READ) {
        _events &= ~EVENT_READ;
        return true;
    }
    return false;
}

inline bool Context::disableWrite() {
    if(_events & EVENT_WRITE) {
        _events &= ~EVENT_WRITE;
        return true;
    }
    return false;
}

inline constexpr void Context::checkHardCode() {
    static_assert(EPOLL_CTL_NONE != EPOLL_CTL_ADD
                    && EPOLL_CTL_NONE != EPOLL_CTL_DEL
                    && EPOLL_CTL_NONE != EPOLL_CTL_MOD,
                    "check EPOLL_CTL_NONE");
}

inline uint64_t Context::hashcode() const {
    if(_hashcode) return _hashcode;
    return _hashcode = (uint64_t(socket.fd()) << 32)
        ^ uint64_t(address.rawIp())
        ^ (uint64_t(address.rawPort()) << 24);
}

inline const char* Context::networkInfo() const {
    switch(_nState) {
        case NetworkState::CONNECTING: return "CONNECTING";
        case NetworkState::CONNECTED: return "CONNECTED";
        case NetworkState::DISCONNECTING: return "DISCONNECTING";
        case NetworkState::DISCONNECTED: return "DISCONNECTED";
        default: return "?";
    }
}

inline char Context::eventStateInfo() const {
    switch(_eState) {
        case EventState::NEW: return 'N';
        case EventState::ADDED: return 'A';
        case EventState::DELETED: return 'D';
        default : return '?';
    }
}

inline char Context::eventsInfo() const {
    switch(_events) {
        case EVENT_NONE: return 'N';
        case EVENT_READ: return 'R';
        case EVENT_WRITE: return 'W';
        case EVENT_READ | EVENT_WRITE: return 'M'; // Mixed
        default : return '?';
    }
}

inline const std::string& Context::simpleInfo() const {
    if(_cachedInfo.length()) return _cachedInfo;
    return _cachedInfo = "[" + std::to_string(socket.fd()) + ", ("
        + address.toString() + ")]";
}

} // fluent
#endif