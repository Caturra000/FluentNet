#ifndef __MUTTY_NET_MULTIPLEXER_H__
#define __MUTTY_NET_MULTIPLEXER_H__
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "../throws/Exceptions.h"
#include "Context.h"
namespace mutty {

// no any looper?
class Multiplexer {
public:
    using Token = size_t;
    // must be pointer (epoll_event.data.ptr)
    using Bundle = std::pair<Token, Context>*;
public:
    void poll(std::chrono::milliseconds timeout) {
        int count = ::epoll_wait(_epollFd, _events.data(), _events.size(), timeout.count());
        if(count < 0) throw EpollWaitException(errno);
        dispatchActiveContext(count);
        if(_events.size() == count) {
            _events.reserve(_events.size() << 1);
        }
    }

    void dispatchActiveContext(int count) {
        // TODO remove
        // for(int i = 0; i < count; ++i) {
        //     auto ctx = static_cast<Context*>(_events[i].data.ptr);
        //     auto revent = _events[i].events;
        //     dispatch(ctx, revent);
        // }

        for(int i = 0; i < count; ++i) {
            // move to map by index
            size_t index = static_cast<std::pair<Token, Context>*>(_events[i].data.ptr)->first;
            _evnetVectors[index].emplace_back(_events[i]);
        }
    }

    // TODO call directly
    void dispatch(Context *ctx, int revent) {
        // // MUTTY_LOG_DEBUG("multiplexer dispatch: context_fd =", ctx->fd(), "revent_mask = ", revent);
        // if((revent & POLLHUP) && !(revent & POLLIN)) {
        //     ctx->sendCloseMessage();
        // }
        // if(revent & (POLLERR | POLLNVAL)) {
        //     ctx->sendErrorMessage();
        // }
        // if(revent & (POLLIN | POLLPRI | POLLRDHUP)) {
        //     ctx->sendReadMessage();
        // }
        // if(revent & POLLOUT) {
        //     ctx->sendWriteMessage();
        // }
    }

    void update(int operation, Context *ctx) {
        // epoll_event event {ctx->events(), ctx};
        // if(::epoll_ctl(_epollFd, operation, ctx->fd(), &event)) {
        //     throw EpollControlException(errno);
        // }
    }

    Token token() {
        _evnetVectors.emplace_back();
        return _token++;
    }

    std::vector<epoll_event>& visit(Token token) {
        return _evnetVectors[token];
    }

    Multiplexer()
        : _epollFd(::epoll_create1(EPOLL_CLOEXEC)),
          _events(16) {
        if(_epollFd < 0) {
            throw EpollCreateException(errno);
        }
    }

    ~Multiplexer() { ::close(_epollFd); }

    Multiplexer(const Multiplexer&) = delete;
    Multiplexer(Multiplexer&&) = default;
    Multiplexer& operator=(const Multiplexer&) = delete;
    Multiplexer& operator=(Multiplexer&&) = default;

private:
    int _epollFd;
    std::vector<epoll_event> _events;
    std::vector<std::vector<epoll_event>> _evnetVectors;
    Token _token {0};
};

}
#endif