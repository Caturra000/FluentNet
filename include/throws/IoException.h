#ifndef __MUTTY_THROWS_IO_EXCEPTION_H__
#define __MUTTY_THROWS_IO_EXCEPTION_H__
#include "ErrnoException.h"
namespace mutty {

class IoException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // mutty
#endif