#ifndef __FLUENT_APP_SERVER_H__
#define __FLUENT_APP_SERVER_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../network/Context.h"
#include "../network/Acceptor.h"
#include "../network/Multiplexer.h"
#include "../handler/Handler.h"
namespace fluent {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer;

using Server = BaseServer<std::function<void(Context*)>,
                          std::function<void(Context*)>,
                          std::function<void(Context*)>>;

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
makeServer(const InetAddress      &address,
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
    using HandlerType = Handler<ConnectCallback, MessageCallback, CloseCallback>;

    // unique_ptr makes Context pointer safe in vector rescale and std::swap
    // token can be accessible in global(server group) multiplexer
    // TODO O(1) GC
    using PoolType = std::vector<std::unique_ptr<std::pair<Multiplexer::Token, Context>>>;

public:
    void run() {
        for(; !_stop; ) {
            if(!_isOuterMultiplexer) {
                _multiplexer->poll(std::chrono::milliseconds::zero());
            }
            accept();
            _handler.handleEvents(_token);
            _looper.loop();
        }
    }

    void ready() {
        _acceptor.start();
    }

    void stop() {
        _stop = true;
    }

    void onConnect(ConnectCallback callback) {
        _handler.onConnect(std::move(callback));
    }

    void onMessage(MessageCallback callback) {
        _handler.onMessage(std::move(callback));
    }

    void onClose(CloseCallback callback) {
        _handler.onClose(std::move(callback));
    }

    BaseServer(InetAddress address)
        : _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false),
          _token(_multiplexer->token()),
          _acceptor(&_looper, address),
          _handler(_multiplexer.get()) {}

    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer)
        : _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true),
          _token(_multiplexer->token()),
          _acceptor(&_looper, address),
          _handler(_multiplexer.get()) {}

    BaseServer(InetAddress address,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false),
          _token(_multiplexer->token()),
          _acceptor(&_looper, address),
          _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true),
          _token(_multiplexer->token()),
          _acceptor(&_looper, address),
          _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

    ~BaseServer() = default;
    BaseServer(const BaseServer&) = delete;
    BaseServer(BaseServer&&) = default;
    BaseServer& operator=(const BaseServer&) = delete;
    BaseServer& operator=(BaseServer&&) = default;

private:
    void accept() {
        if(_acceptor.accept()) {
            std::pair<InetAddress, Socket> contextArguments = _acceptor.aceeptResult();
            auto &address = contextArguments.first;
            auto &socket = contextArguments.second;
            _connections.emplace_back(
                std::make_unique<std::pair<Multiplexer::Token, Context>>(
                    _token, Context(&_looper, address, std::move(socket))));
            Multiplexer::Bundle bundle = _connections.back().get();
            _handler.handleNewContext(bundle);
        }
    }

private:
    Looper _looper;

    std::shared_ptr<Multiplexer> _multiplexer;
    bool _isOuterMultiplexer;
    const Multiplexer::Token _token;

    Acceptor _acceptor;
    HandlerType _handler;
    PoolType _connections;

    bool _stop {false};
};

} // fluent
#endif