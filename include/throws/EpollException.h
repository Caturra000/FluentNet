#ifndef __MUTTY_THROWS_EPOLL_EXCEPTION_H__
#define __MUTTY_THROWS_EPOLL_EXCEPTION_H__
#include "ErrnoException.h"
namespace mutty {

class EpollException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // mutty
#endif