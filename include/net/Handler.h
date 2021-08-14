#ifndef __MUTTY_NET_HANDLER_H__
#define __MUTTY_NET_HANDLER_H__
#include "bits/stdc++.h"
#include "../throws/Exceptions.h"
#include "Context.h"
namespace mutty {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class Handler {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;

public:
    void handleRead(Context *context) {
        // assert(context != nullptr);
        ssize_t n = context->input.readFrom(context->socket.fd());
        if(n > 0) {
            _messageCallback(context);
        } else if(n == 0) {
            // TODO add: const char *callbackReason / size_t reasonCode in Context
            // TODO handleClose(context, "reason...")
            handleClose(context);
        } else {
            handleError(context);
        }
    }

    void handleWrite(Context *context) {
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

    void handleClose(Context *context) {
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
    void handleError(Context *context) {
        try {
            throw MuttyException("error callback");
        } catch(...) {
            context->exception = std::current_exception();
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
    Handler() = default;
    Handler(ConnectCallback connectCallback,
            MessageCallback messageCallback,
            CloseCallback   closeCallback)
        : _connectCallback(std::move(connectCallback)),
          _messageCallback(std::move(messageCallback)),
          _closeCallback(std::move(closeCallback)) {}

    ~Handler() = default;

    Handler(const Handler&) = default;
    Handler(Handler&&) = default;
    Handler& operator=(const Handler&) = default;
    Handler& operator=(Handler&&) = default;

private:
    ConnectCallback _connectCallback;
    MessageCallback _messageCallback;
    CloseCallback   _closeCallback;
};

} // mutty
#endif