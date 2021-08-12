#ifndef __MUTTY_THROWS_SOCKET_BIND_EXCEPTION_H__
#define __MUTTY_THROWS_SOCKET_BIND_EXCEPTION_H__
#include "SocketException.h"
namespace mutty {

class SocketBindException: public SocketException {
public:
    static constexpr const char *TAG = "socket bind exception";
    using SocketException::SocketException;
    SocketBindException(int err): SocketException(TAG, err) {}
};

} // mutty
#endif