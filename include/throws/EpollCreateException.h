#ifndef __MUTTY_THROWS_EPOLL_CREATE_EXCEPTION_H__
#define __MUTTY_THROWS_EPOLL_CREATE_EXCEPTION_H__
#include "EpollException.h"
namespace mutty {

class EpollCreateException: public EpollException {
public:
    static constexpr const char *TAG = "epoll create exception";
    using EpollException::EpollException;
    EpollCreateException(int err): EpollException(TAG, err) {}
};

} // mutty
#endif