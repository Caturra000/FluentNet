#ifndef __MUTTY_THROWS_SOCKET_LISTEN_EXCEPTION_H__
#define __MUTTY_THROWS_SOCKET_LISTEN_EXCEPTION_H__
#include "SocketException.h"
namespace mutty {

class SocketListenException: public SocketException {
public:
    static constexpr const char *TAG = "socket listen exception";
    using SocketException::SocketException;
    SocketListenException(int err): SocketException(TAG, err) {}
};

} // mutty
#endif