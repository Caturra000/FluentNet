#ifndef __FLUENT_APP_CLIENT_H__
#define __FLUENT_APP_CLIENT_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../network/Multiplexer.h"
#include "../network/Connector.h"
#include "../network/Pool.h"
#include "../handler/Handler.h"
namespace fluent {

// a client with parameterized types
// it is needed to be specified all the templated types
// for reasons of higher performance and traits
// see `Client` in common cases
template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseClient;

// applicable in most sitatuions
using Client = BaseClient<std::function<void(Context*)>,
                          std::function<void(Context*)>,
                          std::function<void(Context*)>>;


template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseClient {
public:
    // help traits
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;
    using HandlerType = Handler<ConnectCallback, MessageCallback, CloseCallback>;

public:

    // run client's looping
    // keep running until stop()
    void run();

    // activate reactor and run a batch of tasks
    // then return to your application
    void batch();

    // connect to specified inetaddress
    // actually it is async-connect()
    // it creates an in-loop future to generate a connection context
    // and wont block your application's control flow
    Future<Context*> connect(InetAddress address);

    // callback: T(context*), no restriction on the return type T
    template <typename F>
    auto connect(InetAddress address, F &&callback) -> Future<typename FunctionTraits<F>::ReturnType>;

    // simply stop looping
    void stop() { _stop = true; }

    Looper* looper() { return &_looper; }

    /// register callbacks

    void onConnect(ConnectCallback callback) { _handler.onConnect(std::move(callback)); }
    void onMessage(MessageCallback callback) { _handler.onMessage(std::move(callback)); }
    void onClose(CloseCallback callback) { _handler.onClose(std::move(callback)); }

// basic attibutes
public:
    BaseClient();
    BaseClient(std::shared_ptr<Multiplexer> multiplexer);
    BaseClient(ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    BaseClient(std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    ~BaseClient() = default;
    BaseClient(const BaseClient&) = delete;
    BaseClient(BaseClient&&) = default;
    BaseClient& operator=(const BaseClient&) = delete;
    BaseClient& operator=(BaseClient&&) = default;

// about events
private:
    // owned looper for event-driven cycles
    Looper _looper;

    // demultiplex and dispatch events
    std::shared_ptr<Multiplexer> _multiplexer;

    // flag for Multiplexer optimization
    // see batch() in details
    bool _isOuterMultiplexer;

    // identifier in global environment
    // passed to handler
    const Multiplexer::Token _token;

// user-level processing
private:
    // an async-connector with promise/future pattern
    Connector _connector;

    // a handler of parameterized type
    // in common cases, it is Handler<std::function...>
    HandlerType _handler;

    // a reusable connection pool with weak-reference marking algorithm
    // connection index may be changed in every step
    Pool _connections;

    bool _stop {false};
};

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseClient<ConnectCallback, MessageCallback, CloseCallback>::run() {
    for(_stop = false; !_stop; ) batch();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseClient<ConnectCallback, MessageCallback, CloseCallback>::batch() {
    if(!_isOuterMultiplexer) {
        _multiplexer->poll(std::chrono::milliseconds::zero());
    }
    _handler.handleEvents(_token);
    _looper.loop();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline Future<Context*> BaseClient<ConnectCallback, MessageCallback, CloseCallback>::connect(InetAddress address) {
    return _connector.connect(address)
        .then([this](std::pair<InetAddress, Socket> &&contextArgs) {
            auto &address = contextArgs.first;
            auto &socket = contextArgs.second;
            Multiplexer::Bundle bundle = _connections.emplace(_token, &_looper, address, std::move(socket));
            _handler.handleNewContext(bundle);
            auto context = &bundle->second;
            context->ensureLifecycle();
            return context;
        });
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
template <typename F>
inline auto BaseClient<ConnectCallback, MessageCallback, CloseCallback>::connect(InetAddress address, F &&callback)
-> Future<typename FunctionTraits<F>::ReturnType> {
    return _connector.connect(address)
        .then([this, callback = std::forward<F>(callback)](std::pair<InetAddress, Socket> &&contextArgs) {
            auto &address = contextArgs.first;
            auto &socket = contextArgs.second;
            Multiplexer::Bundle bundle = _connections.emplace(_token, &_looper, address, std::move(socket));
            _handler.handleNewContext(bundle);
            auto context = &bundle->second;
            context->ensureLifecycle();
            return callback(context);
        });
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseClient<ConnectCallback, MessageCallback, CloseCallback>::
BaseClient()
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _connector(&_looper),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseClient<ConnectCallback, MessageCallback, CloseCallback>::
BaseClient(std::shared_ptr<Multiplexer> multiplexer)
    : _multiplexer(std::move(multiplexer)),
      _isOuterMultiplexer(true),
      _token(_multiplexer->token()),
      _connector(&_looper),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseClient<ConnectCallback, MessageCallback, CloseCallback>::
BaseClient(ConnectCallback connectCallback,
           MessageCallback messageCallback,
           CloseCallback   closeCallback)
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _connector(&_looper),
      _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseClient<ConnectCallback, MessageCallback, CloseCallback>::
BaseClient(std::shared_ptr<Multiplexer> multiplexer,
           ConnectCallback connectCallback,
           MessageCallback messageCallback,
           CloseCallback   closeCallback)
    : _multiplexer(std::move(multiplexer)),
       _isOuterMultiplexer(true),
       _token(_multiplexer->token()),
       _connector(&_looper),
       _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

} // fluent
#endif