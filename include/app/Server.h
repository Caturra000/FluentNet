#ifndef __MUTTY_APP_SERVER_H__
#define __MUTTY_APP_SERVER_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../net/Context.h"
#include "../net/Acceptor.h"
#include "../net/Multiplexer.h"
namespace mutty {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer;

using Server = BaseServer<std::function<void(Context&)>,
                          std::function<void(Context&)>,
                          std::function<void(Context&)>>;

// factory function
// help template argument deduction
template <typename ConnectCallbackForward,
          typename MessageCallbackForward,
          typename CloseCallbackForward,
          /// return type
          typename ConnectCallback = typename std::remove_reference<ConnectCallbackForward>::type,
          typename MessageCallback = typename std::remove_reference<MessageCallbackForward>::type,
          typename CloseCallbackd  = typename std::remove_reference<CloseCallbackForward>::type>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallbackd>
makeServer(InetAddress address,
           ConnectCallbackForward &&connectCallback,
           MessageCallbackForward &&messageCallback,
           CloseCallbackForward   &&closeCallback) {
    return BaseServer<ConnectCallback, MessageCallback, CloseCallbackd>(
            address,
            std::forward<ConnectCallbackForward>(connectCallback),
            std::forward<MessageCallbackForward>(messageCallback),
            std::forward<CloseCallbackForward>(closeCallback)
        );
}

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;

public:
    void run() {
        for(; !_stop; ) {
            if(!_isOuterMultiplexer) {
                _multiplexer->poll(std::chrono::milliseconds::zero());
            }
            _looper.loop();
        }
    }

    BaseServer(InetAddress address)
        : _acceptor(&_looper, address),
          _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false) {}

    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer)
        : _acceptor(&_looper, address),
          _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true) {}

    BaseServer(InetAddress address,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _acceptor(&_looper, address),
          _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false),
          _connectCallback(std::move(connectCallback)),
          _messageCallback(std::move(messageCallback)),
          _closeCallback(std::move(closeCallback)) {}

    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _acceptor(&_looper, address),
          _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true),
          _connectCallback(std::move(connectCallback)),
          _messageCallback(std::move(messageCallback)),
          _closeCallback(std::move(closeCallback)) {}

    ~BaseServer() = default;
    BaseServer(const BaseServer&) = delete;
    BaseServer(BaseServer&&) = default;
    BaseServer& operator=(const BaseServer&) = delete;
    BaseServer& operator=(BaseServer&&) = default;

    void onConnect(ConnectCallback callback) {
        _connectCallback = std::move(callback);
    }

    void onMessage(MessageCallback callback) {
        _messageCallback = std::move(callback);
    }

    void onClose(CloseCallback callback) {
        _closeCallback = std::move(callback);
    }

private:
    Looper _looper;
    Acceptor _acceptor;
    std::shared_ptr<Multiplexer> _multiplexer;
    bool _isOuterMultiplexer;
    bool _stop {false};
    ConnectCallback _connectCallback;
    MessageCallback _messageCallback;
    CloseCallback _closeCallback;
};

} // mutty
#endif