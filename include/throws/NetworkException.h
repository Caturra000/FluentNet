#ifndef __MUTTY_THROWS_NETWORK_EXCEPTION_H__
#define __MUTTY_THROWS_NETWORK_EXCEPTION_H__
#include "ErrnoException.h"
namespace mutty {

class NetworkException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // mutty
#endif