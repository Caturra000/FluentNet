#ifndef __MUTTY_APP_CLIENT_H__
#define __MUTTY_APP_CLIENT_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../network/Multiplexer.h"
#include "../network/Connector.h"
#include "../handler/Handler.h"
namespace mutty {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseClient;

using Client = BaseClient<std::function<void(Context*)>,
                          std::function<void(Context*)>,
                          std::function<void(Context*)>>;


template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseClient {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;
    using HandlerType = Handler<ConnectCallback, MessageCallback, CloseCallback>;
    using PoolType = std::vector<std::unique_ptr<std::pair<Multiplexer::Token, Context>>>;


public:
    void run() {
        for(; !_stop; ) {
            if(!_isOuterMultiplexer) {
                _multiplexer->poll(std::chrono::milliseconds::zero());
            }
            _handler.handleEvents(_token);
            _looper.loop();
        }
    }

    // connect group
    // future: in client loop
    Future<Context*> connect(InetAddress address) {
        return future = _connector.connect(address)
            .then([this](std::pair<InetAddress, Socket> &&contextArgs) {
                auto &address = contextArgs.first;
                auto &socket = contextArgs.second;
                _connections.emplace_back(
                    std::make_unique<std::pair<Multiplexer::Token, Context>>(
                        _token, Context(&_looper, address, std::move(socket))));
                Multiplexer::Bundle bundle = _connections.back().get();
                _handler.handleNewContext(bundle);
                auto context = &bundle->second;
                return context;
            });
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

    BaseClient()
        : _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false),
          _token(_multiplexer->token()),
          _connector(&_looper),
          _handler(_multiplexer.get()) {}

    BaseClient(std::shared_ptr<Multiplexer> multiplexer)
        : _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true),
          _token(_multiplexer->token()),
          _connector(&_looper),
          _handler(_multiplexer.get()) {}

    BaseClient(ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _multiplexer(std::make_shared<Multiplexer>()),
          _isOuterMultiplexer(false),
          _token(_multiplexer->token()),
          _connector(&_looper),
          _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

    BaseClient(std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback)
        : _multiplexer(std::move(multiplexer)),
          _isOuterMultiplexer(true),
          _token(_multiplexer->token()),
          _connector(&_looper),
          _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

    ~BaseClient() = default;
    BaseClient(const BaseClient&) = delete;
    BaseClient(BaseClient&&) = default;
    BaseClient& operator=(const BaseClient&) = delete;
    BaseClient& operator=(BaseClient&&) = default;

private:
    Looper _looper;

    std::shared_ptr<Multiplexer> _multiplexer;
    bool _isOuterMultiplexer;
    const Multiplexer::Token _token;

    Connector _connector;
    HandlerType _handler;
    PoolType _connections;
    PoolType _readyConnections;

    bool _stop {false};
};

} // mutty
#endif