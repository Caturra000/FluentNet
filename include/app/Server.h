#ifndef __FLUENT_APP_SERVER_H__
#define __FLUENT_APP_SERVER_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../network/Context.h"
#include "../network/Acceptor.h"
#include "../network/Multiplexer.h"
#include "../network/Pool.h"
#include "../handler/Handler.h"
namespace fluent {

// a server with parameterized types
// it is needed to be specified all the templated types
// for reasons of higher performance and traits
// see `Server` below in common cases
// and makeServer() for type deduction
template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer;

// applicable in most sitatuions
using Server = BaseServer<std::function<void(Context*)>,
                          std::function<void(Context*)>,
                          std::function<void(Context*)>>;

// factory function
// help template argument deduction
// so you can make() baseservers easier
// TODO: simply passed by value (and std::move)?
template <typename ConnectCallbackForward,
          typename MessageCallbackForward,
          typename CloseCallbackForward,
          /// return type
          typename ConnectCallback = typename std::remove_reference<ConnectCallbackForward>::type,
          typename MessageCallback = typename std::remove_reference<MessageCallbackForward>::type,
          typename CloseCallback  = typename std::remove_reference<CloseCallbackForward>::type>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>
makeServer(const InetAddress      &address,
           ConnectCallbackForward &&connectCallback,
           MessageCallbackForward &&messageCallback,
           CloseCallbackForward   &&closeCallback) {
    return BaseServer<ConnectCallback, MessageCallback, CloseCallback>(
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
// alias
public:
    // help traits
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;
    using HandlerType = Handler<ConnectCallback, MessageCallback, CloseCallback>;

// for event-driven
public:
    // run server's looping
    // keep running until stop()
    void run();

    // activate reactor and run a batch of tasks
    // then return to your application
    void batch();

    // set this server to ready state
    // that is, switch NEW to LISTEN
    void ready() { _acceptor.start(); }

    // simply stop looping
    // server will stop looping within a certain number of executions
    // NOTE: the next task may still be continued
    void stop() { _stop = true; }

    // get owned looper
    // maybe helpful for application scalability
    // for example: create `fluent::future`s through this looper in user-defined callbacks
    Looper* looper() { return &_looper; }

    /// register callbacks

    void onConnect(ConnectCallback callback) { _handler.onConnect(std::move(callback)); }
    void onMessage(MessageCallback callback) { _handler.onMessage(std::move(callback)); }
    void onClose(CloseCallback callback) { _handler.onClose(std::move(callback)); }

// basic attributes
public:
    BaseServer(InetAddress address);
    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer);
    BaseServer(InetAddress address,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    ~BaseServer() = default;
    BaseServer(const BaseServer&) = delete;
    BaseServer(BaseServer&&) = default;
    BaseServer& operator=(const BaseServer&) = delete;
    BaseServer& operator=(BaseServer&&) = default;

private:
    // an embedded ACCEPT routine
    // called when switched to LISTEN state
    // apply acceptor/connector pattern to event loop
    void accept();

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
    // helper class for acceptor/connector pattern
    // see BaseServer::accept()
    Acceptor _acceptor;

    // a handler of parameterized type
    // in common cases, it is Handler<std::function...>
    HandlerType _handler;

    // a reusable connection pool with weak-reference marking algorithm
    // connection index may be changed in every step
    Pool _connections;

    bool _stop {false};
};

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseServer<ConnectCallback, MessageCallback, CloseCallback>::run() {
    for(_stop = false; !_stop; ) batch();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseServer<ConnectCallback, MessageCallback, CloseCallback>::batch() {
    if(!_isOuterMultiplexer) {
        _multiplexer->poll(std::chrono::milliseconds::zero());
    }
    accept();
    _handler.handleEvents(_token);
    _looper.loop();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address)
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address,
           std::shared_ptr<Multiplexer> multiplexer)
    : _multiplexer(std::move(multiplexer)),
      _isOuterMultiplexer(true),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address,
           ConnectCallback connectCallback,
           MessageCallback messageCallback,
           CloseCallback   closeCallback)
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
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

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseServer<ConnectCallback, MessageCallback, CloseCallback>::accept() {
    if(_acceptor.accept()) {
        std::pair<InetAddress, Socket> contextArguments = _acceptor.aceeptResult();
        auto &address = contextArguments.first;
        auto &socket = contextArguments.second;
        Multiplexer::Bundle bundle = _connections.emplace(_token, &_looper, address, std::move(socket));
        _handler.handleNewContext(bundle);
    }
}

} // fluent
#endif