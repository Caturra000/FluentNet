#ifndef __MUTTY_THROWS_READ_EXCEPTION_H__
#define __MUTTY_THROWS_READ_EXCEPTION_H__
#include "IoException.h"
namespace mutty {

class ReadException: public IoException {
public:
    static constexpr const char *TAG = "read exception";
    using IoException::IoException;
    ReadException(int err): IoException(TAG, err) {}
};

} // mutty
#endif