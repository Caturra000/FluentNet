#ifndef __FLUENT_HANDLER_HANDLER_H__
#define __FLUENT_HANDLER_HANDLER_H__
#include <bits/stdc++.h>
#include "../network/Multiplexer.h"
#include "../network/Context.h"
#include "HandlerBase.h"
namespace fluent {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class Handler
    : public HandlerBase<Handler<ConnectCallback, MessageCallback, CloseCallback>,
                         Multiplexer::Token,
                         Multiplexer::Bundle> {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;

    using Token = Multiplexer::Token;
    using Bundle = Multiplexer::Bundle;

    using ThisType = Handler<ConnectCallback, MessageCallback, CloseCallback>;
    using Base = HandlerBase<ThisType, Token, Bundle>;

public:
    // side effect: event vector .clear()
    void handleEvents(Token token);

    void handleNewContext(Bundle bundle);

// require
public:
    void handleRead(Bundle bundle);

    // eusure: write event
    void handleWrite(Bundle bundle);

    // connected/disconnecting -> disconnecting -> disconnected
    // if call shutdown manually -> disconnecting
    void handleClose(Bundle bundle);


    // TODO handleError(Context *context, const char *exceptionMessage) // dont use std::string, .c_str() is unsafe
    // TODO FluentException +std::string
    void handleError(Bundle bundle);
    void handleError(Bundle bundle, const char *errorMessage);

// register
public:
    void onConnect(ConnectCallback callback) { _connectCallback = std::move(callback); }
    void onMessage(MessageCallback callback) { _messageCallback = std::move(callback); }
    void onClose(CloseCallback callback) { _closeCallback = std::move(callback); }

public:
    Handler() = delete;
    Handler(Multiplexer *multiplexer)
        : _multiplexer(multiplexer) {}
    Handler(Multiplexer *multiplexer,
            ConnectCallback connectCallback,
            MessageCallback messageCallback,
            CloseCallback   closeCallback)
        : _multiplexer(multiplexer),
          _connectCallback(std::move(connectCallback)),
          _messageCallback(std::move(messageCallback)),
          _closeCallback(std::move(closeCallback)) {}

    ~Handler() = default;

    Handler(const Handler&) = default;
    Handler(Handler&&) = default;
    Handler& operator=(const Handler&) = default;
    Handler& operator=(Handler&&) = default;

private:
    Multiplexer     *_multiplexer;
    ConnectCallback _connectCallback;
    MessageCallback _messageCallback;
    CloseCallback   _closeCallback;
};

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleEvents(Token token) {
    auto &eventVector = _multiplexer->visit(token);
    for(auto &event : eventVector) {
        auto bundle = static_cast<Bundle>(event.data.ptr);
        auto revent = event.events;
        try {
            Base::handleEvent(bundle, revent);
        } catch(...) {
            // Log...
            auto context = &bundle->second;
            context->exception = std::current_exception();
            // TODO _exceptionCallback(context);
        }
    }
    eventVector.clear();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleNewContext(Bundle bundle) try {
    auto context = &bundle->second;
    // ensure
    context->_multiplexer = _multiplexer;
    context->_bundle = bundle;
    // assert CONNECTING
    context->_nState = Context::NetworkState::CONNECTED;
    context->enableRead();
    Context::EpollOperationHint operation;
    if((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE) {
        _multiplexer->update(operation, bundle);
    }
    _connectCallback(context);
} catch(...) {
    // Log...
    auto context = &bundle->second;
    context->exception = std::current_exception();
    // TODO _exceptionCallback(context);
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleRead(Bundle bundle) {
    auto context = &bundle->second;
    ssize_t n = context->input.readFrom(context->socket.fd());
    if(n > 0) {
        _messageCallback(context);
    } else if(n == 0) {
        // TODO add: const char *callbackReason / size_t reasonCode in Context
        // TODO handleClose(context, "reason...")
        handleClose(bundle);
    } else {
        handleError(bundle);
    }
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleWrite(Bundle bundle) {
    auto context = &bundle->second;
    if(context->writeEventEnabled()) {
        ssize_t n = context->output.writeTo(context->socket.fd());
        // assert n >= 0
        if(n > 0 && context->output.unread() == 0) {
            Context::EpollOperationHint operation;
            if(context->disableWrite()
                    && ((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE)) {
                _multiplexer->update(operation, bundle);
            }
            // writeComplete
            // FIXME latency
            context->_sendCompleteCounter += context->_readyToCompleteCounter;
            context->_readyToCompleteCounter = 0;
        }
    }
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleClose(Bundle bundle) {
    auto context = &bundle->second;
    if(context->isConnected() || context->isDisConnecting()) {
        // try to shutdown if force close
        context->shutdown();
        // shouble be DISCONNECTING
        context->disableRead();
        context->disableWrite();
        Context::EpollOperationHint operation;
        if((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE) {
            _multiplexer->update(operation, bundle);
        }
        context->_nState = Context::NetworkState::DISCONNECTED;
        _closeCallback(context);
    }
}


template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleError(Bundle bundle) {
    // auto context = &bundle->second;
    // try {
    //     throw FluentException("error callback");
    // } catch(...) {
    //     context->exception = std::current_exception();
    // }
    throw FluentException("error callback");
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleError(Bundle bundle, const char *errorMessage) {
    throw FluentException(errorMessage);
}

} // fluent
#endif