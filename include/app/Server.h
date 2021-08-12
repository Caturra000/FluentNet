#ifndef __MUTTY_APP_SERVER_H__
#define __MUTTY_APP_SERVER_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../net/Context.h"
namespace mutty {


// TODO template <F> class BaseServer

class Server {

private:
    Looper _looper;
    Acceptor _acceptor;
    std::function<void(Context&)> _connectionCallback;
    std::function<void(Context&)> _messageCallback;
    std::function<void(Context&)> _closeCallback;
};

} // mutty
#endif