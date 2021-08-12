#ifndef __MUTTY_NET_CONTEXT_H__
#define __MUTTY_NET_CONTEXT_H__
#include <bits/stdc++.h>
#include "../future/Looper.h"
#include "InetAddress.h"
#include "Socket.h"
namespace mutty {


class Context {

// private:
public:
    Looper *looper;
    InetAddress address;
    Socket socket;
    std::exception_ptr exception;
    // Object any;
};

} // mutty
#endif