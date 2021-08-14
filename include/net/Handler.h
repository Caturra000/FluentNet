#ifndef __MUTTY_NET_HANDLER_H__
#define __MUTTY_NET_HANDLER_H__
#include "bits/stdc++.h"
#include "../throws/Exceptions.h"
#include "Context.h"
#include "Multiplexer.h"
namespace mutty {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class Handler {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;

    using Token = Multiplexer::Token;
    using Bundle = Multiplexer::Bundle;

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

    void handleWrite(Bundle bundle) {
        auto context = &bundle->second;
        if(context->writeEventEnabled()) {
            ssize_t n = context->output.writeTo(context->socket.fd());
            if(n > 0 && !context->output.unread()) {
                Context::EpollOperationHint operation;
                if(context->disableWrite()
                        && (operation = context->updateEventState() != Context::EPOLL_CTL_NONE)) {
                    // TODO
                    // epoll->update(operation, context)
                }
                // writeComplete
            } else if(n < 0) {
                // warn
            }
        }
    }

    void handleClose(Bundle bundle) {
        auto context = &bundle->second;
        if(context->isConnected() || context->isDisConnecting()) {
            // try to shutdown if force close
            context->shutdown();
            // shouble be DISCONNECTING
            context->disableRead();
            context->disableWrite();
            // TODO epoll->update()

            // _context->async([this, context] {
            //     context->setDisConnected();
            //     _closeCallback();
            // });
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

    void handleStart(Bundle bundle) {
        auto context = &bundle->second;
        // assert CONNECTING
        context->_nState = Context::NetworkState::CONNECTED;
        context->enableRead();
        // TODO poll update
        Context::EpollOperationHint operation;

        _connectCallback(context);
    }

    // side effect: event vector .clear()
    void handleEvents(Token token) {
        auto &eventVector = _multiplexer->visit(token);
        for(auto &event : eventVector) {
            Bundle bundle = static_cast<Bundle>(event.data.ptr);
            auto revent = event.events;
            handleEvent(bundle, revent);
        }
        eventVector.clear();
    }

    void handleEvent(Bundle bundle, uint32_t revent) {
        if((revent & POLLHUP) && !(revent & POLLIN)) {
            handleClose(bundle);
        }
        if(revent & (POLLERR | POLLNVAL)) {
            handleError(bundle);
        }
        if(revent & (POLLIN | POLLPRI | POLLRDHUP)) {
            handleRead(bundle);
        }
        if(revent & POLLOUT) {
            handleWrite(bundle);
        }
    }

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