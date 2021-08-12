#ifndef __MUTTY_THROWS_WRITE_EXCEPTION_H__
#define __MUTTY_THROWS_WRITE_EXCEPTION_H__
#include "IoException.h"
namespace mutty {

class WriteException: public IoException {
public:
    static constexpr const char *TAG = "write exception";
    using IoException::IoException;
    WriteException(int err): IoException(TAG, err) {}
};

} // mutty
#endif