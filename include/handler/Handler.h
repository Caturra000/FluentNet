#ifndef __MUTTY_HANDLER_HANDLER_H__
#define __MUTTY_HANDLER_HANDLER_H__
#include <bits/stdc++.h>
#include "../network/Multiplexer.h"
#include "../network/Context.h"
#include "HandlerBase.h"
namespace mutty {

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
    void handleEvents(Token token) {
        auto &eventVector = _multiplexer->visit(token);
        for(auto &event : eventVector) {
            auto bundle = static_cast<Bundle>(event.data.ptr);
            auto revent = event.events;
            Base::handleEvent(bundle, revent);
        }
        eventVector.clear();
    }

    void handleNewContext(Bundle bundle) {
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
    }

// require
public:
    void handleRead(Bundle bundle) {
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

    // eusure: write event
    void handleWrite(Bundle bundle) {
        auto context = &bundle->second;
        if(context->writeEventEnabled()) {
            ssize_t n = context->output.writeTo(context->socket.fd());
            if(n > 0 && context->output.unread() == 0) {
                Context::EpollOperationHint operation;
                if(context->disableWrite()
                        && ((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE)) {
                    _multiplexer->update(operation, bundle);
                }
                // writeComplete
            } else if(n < 0) {
                // warn
            }
        }
    }

    // connected/disconnecting -> disconnecting -> disconnected
    // if call shutdown manually -> disconnecting
    void handleClose(Bundle bundle) {
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


    // TODO handleError(Context *context, const char *exceptionMessage) // dont use std::string, .c_str() is unsafe
    // TODO MuttyException +std::string
    void handleError(Bundle bundle) {
        auto context = &bundle->second;
        try {
            throw MuttyException("error callback");
        } catch(...) {
            context->exception = std::current_exception();
        }
    }

// register
public:
    void onConnect(ConnectCallback callback) {
        _connectCallback = std::move(callback);
    }

    void onMessage(MessageCallback callback) {
        _messageCallback = std::move(callback);
    }

    void onClose(CloseCallback callback) {
        _closeCallback = std::move(callback);
    }

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

} // mutty
#endif