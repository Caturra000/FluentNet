#ifndef __MUTTY_THROWS_SOCKET_ACCEPT_EXCEPTION_H__
#define __MUTTY_THROWS_SOCKET_ACCEPT_EXCEPTION_H__
#include "SocketException.h"
namespace mutty {

class SocketAcceptException: public SocketException {
public:
    static constexpr const char *TAG = "socket accept exception";
    using SocketException::SocketException;
    SocketAcceptException(int err): SocketException(TAG, err) {}
};

} // mutty
#endif