#ifndef __MUTTY_THROWS_EPOLL_WAIT_EXCEPTION_H__
#define __MUTTY_THROWS_EPOLL_WAIT_EXCEPTION_H__
#include "EpollException.h"
namespace mutty {

class EpollWaitException: public EpollException {
public:
    static constexpr const char *TAG = "epoll create exception";
    using EpollException::EpollException;
    EpollWaitException(int err): EpollException(TAG, err) {}
};

} // mutty
#endif