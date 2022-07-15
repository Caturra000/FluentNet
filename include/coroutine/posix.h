#pragma once
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>
#include <map>
#include <memory>
#include <iostream>
#include "Coroutine.h"
#include "Utilities.h"

// posix.h文件提供一些常见POSIX接口的协程改造
// 比如用co::read替代::read
//
// 并不打算高度封装
//
// 这里做些规约：每个协程处理各自的fd，不共享给其它协程使用
// 这样我可以方便地管理event

namespace fluent {
namespace co {

struct PollConfig;
struct Event;

/// interface

ssize_t read(int fd, void *buf, size_t size);
ssize_t write(int fd, void *buf, size_t size);
int connect(int fd, const sockaddr *addr, socklen_t len);
int accept4(int fd, sockaddr *addr, socklen_t *len, int flags);

unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

PollConfig& getPollConfig();
void loop();










/// implement

struct Event {
    enum Type {
        READ = 0,
        WRITE = 1,
        ERROR = 2,
        SIZE
    };
    // 0: POLLIN
    // 1: POLLOUT
    // 2: POLLERR
    using RoutineTable = std::array<std::shared_ptr<Coroutine>, 3>;
    RoutineTable routines;
    epoll_event event {};
};

struct PollConfig {
    // key: fd;
    // value: epoll_event
    using EventList = std::unordered_map<int, Event>;
    using Milliseconds = std::chrono::milliseconds;

    constexpr static auto DEFAULT_TIMEOUT = std::chrono::milliseconds(1000);
    constexpr static auto DEFAULT_CONNECT_RETRIES = size_t(8);

    int          epfd;
    Milliseconds timeout {DEFAULT_TIMEOUT};
    EventList    events;
    size_t       connectRetries {DEFAULT_CONNECT_RETRIES};

    explicit PollConfig(int fd = -1): epfd(fd) {
        if(epfd < 0) {
            epfd = ::epoll_create1(EPOLL_CLOEXEC);
        }
        if(epfd < 0) {
            throw std::runtime_error("poll config");
        }
    }
    ~PollConfig() { ::close(epfd); }
    PollConfig(const PollConfig&) = delete;
    PollConfig& operator=(const PollConfig&) = delete;
};

inline PollConfig& getPollConfig() {
    static thread_local PollConfig config;
    return config;
}

// internal
inline auto addEvent(int fd, Event::Type type)
-> PollConfig::EventList::iterator {
    auto &config = getPollConfig();
    auto &events = config.events;
    int epfd = config.epfd;
    auto iter = events.find(fd);
    int op;
    bool newAdd = false;
    if(iter == events.end()) {
        op = EPOLL_CTL_ADD;
        auto where = events.insert({fd, {}});
        newAdd = true;
        iter = where.first;
        iter->second.event.data.fd = fd;
        auto &slot = iter->second.routines[type];
        slot = Coroutine::current().shared_from_this();
    } else {
        // impossible?
        op = EPOLL_CTL_MOD;
    }
    epoll_event *e = &iter->second.event;
    int newEvent = 0;
    switch(type) {
        case Event::READ:
            newEvent = EPOLLIN;
            break;
        case Event::WRITE:
            newEvent = EPOLLOUT;
            break;
        case Event::ERROR:
            newEvent = EPOLLERR;
            break;
    };
    // duplicate ?
    if(e->events & newEvent) {
        // revert
        if(newAdd) {
            events.erase(fd);
        }
        return events.end();
    }
    e->events |= newEvent;
    if(::epoll_ctl(epfd, op, fd, e)) {
        // std::cerr << "ctl failed: " << strerror(errno) << std::endl;
    }
    return iter;
}

inline ssize_t read(int fd, void *buf, size_t size) {
    // try
    ssize_t ret = ::read(fd, buf, size);
    // if ready
    if(ret > 0) return ret;

    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::READ);

    // 存在重复的关注事件
    // 返回0建议上层重试处理
    // FIXME. -1更好点？
    if(iter == poll.events.end()) {
        return 0;
    }

    this_coroutine::yield();

    // yield back from loop
    ret = ::read(fd, buf, size);
    return ret;
}

inline ssize_t write(int fd, void *buf, size_t size) {
    ssize_t ret;
    ret = ::write(fd, buf, size);
    if(ret > 0) return ret;
    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::WRITE);
    if(iter == poll.events.end()) {
        return 0;
    }
    this_coroutine::yield();
    ret = ::write(fd, buf, size);
    return ret;
}

inline int connect(int fd, const sockaddr *addr, socklen_t len) {
    const size_t maxRetries = getPollConfig().connectRetries;
    size_t retries = 0;

    while(retries < maxRetries) {

        // back-off
        // 0 - 0 - 0 - 1s - 2s - 4s - 8s - 16s - 32s - ...
        // or custom retries
        if(retries++ > 2) {
            co::poll(nullptr, 0, 1024 << (retries - 3));
        }

        int ret;

        ret = ::connect(fd, addr, len);

        auto &poll = getPollConfig();
        auto iter = addEvent(fd, Event::Type::WRITE);
        if(iter == poll.events.end()) {
            errno = EPERM;
            return -1;
        }

        this_coroutine::yield();

        int soerr;
        socklen_t jojo = sizeof(soerr);
        if(::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &jojo)) {
            errno = EPERM;
            return -1;
        }
        switch(soerr) {
            case 0:
            case EINTR:
            case EINPROGRESS:
            case EALREADY:
            case EISCONN:
            {
                sockaddr jojo;
                socklen_t dio = sizeof jojo;
                if(::getpeername(fd, &jojo, &dio)) {
                    // retry
                    continue;
                }
                return 0;
            }
            case EAGAIN:
            case EADDRINUSE:
            case EADDRNOTAVAIL:
            case ENETUNREACH:
            case ECONNREFUSED:
                // retry
                continue;
            default:
                errno = soerr;
                return -1;
        }
        return 0;
    }
    errno = ETIMEDOUT;
    return -1;
}

inline int accept4(int fd, sockaddr *addr, socklen_t *len, int flags) {
    int ret = ::accept4(fd, addr, len, flags);
    if(ret >= 0) return ret;
    auto &poll = getPollConfig();
    auto iter = addEvent(fd, Event::Type::READ);
    // FIXME 这里只允许单个协程对同一fd进行accpet
    if(iter == poll.events.end()) return 0;
    this_coroutine::yield();
    ret = ::accept4(fd, addr, len, flags);
    return ret;
}

inline unsigned int sleep(unsigned int seconds) {
    using namespace std::chrono;
    auto now = [] { return steady_clock::now(); };
    auto delta = [&, start = now()] {
        auto d = duration_cast<std::chrono::seconds>(now() - start);
        return std::max<int64_t>(0, seconds - d.count());
    };

    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerfd < 0) {
        return delta();
    }

    auto defer = [=](auto) { ::close(timerfd);  };
    std::shared_ptr<void> guard {nullptr, defer};

    itimerspec newValue {};
    itimerspec oldValue {};
    newValue.it_value.tv_sec = seconds;
    if(::timerfd_settime(timerfd, 0, &newValue, &oldValue)) {
        return delta();
    }

    auto iter = addEvent(timerfd, Event::Type::READ);

    co::this_coroutine::yield();

    itimerspec retValue {};
    if(::timerfd_gettime(timerfd, &retValue)) {
        return delta();
    }
    return retValue.it_value.tv_sec;
}

inline int usleep(useconds_t usec) {
    // usec is greater than or equal to 1000000.
    // (On systems where that is considered an error.)
    if(usec >= 1000000) {
        errno = EINVAL;
        return -1;
    }
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerfd < 0) {
        // usleep允许的errno太少了
        errno = EINTR;
        return -1;
    }

    auto defer = [=](void*) { ::close(timerfd); };
    std::shared_ptr<void> guard {nullptr, defer};

    itimerspec newValue {};
    itimerspec oldValue {};
    // newValue.it_value.tv_sec = 0;
    newValue.it_value.tv_nsec = usec * 1000;
    if(::timerfd_settime(timerfd, 0, &newValue, &oldValue)) {
        errno = EINTR;
        return -1;
    }

    auto iter = addEvent(timerfd, Event::Type::READ);

    co::this_coroutine::yield();

    itimerspec retValue {};
    if(::timerfd_gettime(timerfd, &retValue)) {
        errno = EINTR;
        return -1;
    }
    int ret = retValue.it_value.tv_nsec / 1000;
    return ret;
}

inline int poll(struct pollfd *fds, nfds_t nfds, int timeout) {

    // TODO 如果有同一fd关注到不同的fds下标，需要poll merge
    // TODO 针对空fds、单个fds的场合其实仍有优化空间，有空再写吧

    int ret;

    // try
    ret = ::poll(fds, nfds, 0);
    if(ret != 0 || timeout == 0) return ret;

    // epoll can wait for epoll itself

    int epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if(epfd < 0) return -1;

    // real timer

    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerfd < 0) return -1;

    itimerspec newValue {};
    itimerspec oldValue {};
    // ms -> s
    newValue.it_value.tv_sec = timeout / 1000;
    // ms -> us(1000) -> ns(1000)
    newValue.it_value.tv_nsec = timeout % 1000 * 1000000;
    if(::timerfd_settime(timerfd, 0, &newValue, &oldValue)) {
        errno = EINTR;
        return -1;
    }

    nfds_t realNfds = nfds + 1;

    // dtor

    auto defer = [=](void*) {
        ::close(epfd);
        ::close(timerfd);
    };
    std::shared_ptr<void> guard {nullptr, defer};

    // add events

    // 在 < 1000 的量级下，map实际表现优于unordered_map
    std::map<nfds_t, int> indices2fd;
    size_t indicesCounter = 0;

    for(nfds_t i = 0; i < nfds; ++i) {
        epoll_event e {};

        size_t id = indicesCounter++;
        indices2fd[id] = fds[i].fd;
        e.data.u64 = id;

        e.events = fds[i].events;
        ::epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i].fd, &e);
    }

    epoll_event timedEvent {};
    size_t timerId = indicesCounter++;
    indices2fd[timerId] = timerfd;
    timedEvent.data.u64 = timerId;
    timedEvent.events = EPOLLIN;
    if(::epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &timedEvent)) {
        return -1;
    }

    addEvent(epfd, Event::Type::READ);
    co::this_coroutine::yield();

    // collect

    epoll_event *revents;

    constexpr static size_t EVENTS_USE_STACK = 1024;
    std::vector<epoll_event> eventsLarge;
    epoll_event eventsSmall[EVENTS_USE_STACK];
    if(realNfds < EVENTS_USE_STACK) {
        ::memset(eventsSmall, 0, sizeof eventsSmall);
        revents = eventsSmall;
    } else {
        eventsLarge.reserve(realNfds);
        revents = eventsLarge.data();
    }

    int realRet = ::epoll_wait(epfd, revents, realNfds, 0);
    if(realRet < 0) return -1;

    ret = realRet;

    for(nfds_t i = 0; i < realRet; ++i) {
        auto &e = revents[i];
        auto index = e.data.u64;
        int fd = indices2fd[index];
        if(fd == timerfd) {
            ret--;
            continue;
        }
        fds[index].revents = e.events;
    }
    return ret;
}

inline void loop() {
    auto &config = getPollConfig();
    // config may change
    // don't get / cache fields outside loop
    for(;;) {
        epoll_event e {};
        auto &eventList = config.events;
        int n = ::epoll_wait(config.epfd, &e, 1, config.timeout.count());
        // TODO 暂不处理errno
        if(n == 1) {
            int fd = e.data.fd;
            auto iter = eventList.find(fd);
            if(iter == eventList.end()) continue;
            ::epoll_ctl(config.epfd, EPOLL_CTL_DEL, fd, nullptr);
            auto routines = std::move(iter->second.routines);
            auto revent = iter->second.event;
            eventList.erase(iter);
            // 为了简化处理
            // 即使已关注的**部分**revent没有到来，也同样进行resume
            // 因为可能存在跨协程操作同一个fd，不处理就会丢失
            // 同时这要求接口必须是nonblock的，yield back处理可能得到err或者0
            //
            // 而且由于协程的性质，不可能同一个协程关注多个事件
            // 如果是按照约定使用，那么到来的事件中肯定只有一个slot是存在event的
            if(/* (revent.events & EPOLLIN) && */routines[Event::READ]) {
                routines[Event::READ]->resume();
            }
            if(/* (revent.events & EPOLLOUT) && */routines[Event::WRITE]) {
                routines[Event::WRITE]->resume();
            }
            if(/* (revent.events & EPOLLERR) && */routines[Event::ERROR]) {
                routines[Event::ERROR]->resume();
            }
        }
        // TODO 处理超时event
    }
}

} // co
} // fluent
