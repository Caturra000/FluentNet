#ifndef __MUTTY_NET_ACCEPTOR_H__
#define __MUTTY_NET_ACCEPTOR_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "InetAddress.h"
#include "Socket.h"
namespace mutty {

class Acceptor {


private:
    Looper *_looper;
    InetAddress _address;
    Socket _listen;
};

} // mutty
#endif