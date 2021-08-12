#ifndef __MUTTY_THROWS_SOCKET_CREATE_EXCEPTION_H__
#define __MUTTY_THROWS_SOCKET_CREATE_EXCEPTION_H__
#include "SocketException.h"
namespace mutty {

class SocketCreateException: public SocketException {
public:
    static constexpr const char *TAG = "socket create exception";
    using SocketException::SocketException;
    SocketCreateException(int err)
        : SocketException(TAG, err) {}
};

} // mutty
#endif