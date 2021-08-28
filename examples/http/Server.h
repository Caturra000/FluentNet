#ifndef __FLUENT_HTTP_SERVER_H__
#define __FLUENT_HTTP_SERVER_H__
#include <bits/stdc++.h>
#include "forward.hpp"
#include "Request.h"
#include "Response.h"
#include "Parser.h"
#include "Stream.h"
namespace fluent {
namespace http {

class Server {
public:
    using RequestCallback = std::function<void(const Request*, Response*)>;
public:
    Server(const InetAddress &address);
    Server(const InetAddress &address,
           std::shared_ptr<Multiplexer> multiplexer);

    ~Server() = default;
    Server(const Server&) = delete;
    Server(Server&&) = default;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = default;

    void onRequest(RequestCallback callback) { _requestCallback = std::move(callback); }

    Looper* looper() { return _server.looper(); }
    void ready() { _server.ready(); }
    void run() { _server.run(); }
    void batch() { _server.batch(); }
    void stop() { _server.stop(); }

private:
    void init();

private:
    fluent::Server _server;
    RequestCallback _requestCallback;
    Parser _parser;
};

inline Server::Server(const InetAddress &address)
    : _server(address) { init(); }

inline Server::Server(const InetAddress &address,
                      std::shared_ptr<Multiplexer> multiplexer)
    : _server(address, std::move(multiplexer)) { init(); }

inline void Server::init() {
    _server.onConnect([](fluent::Context *context) {
        context->any = std::make_tuple(Stream{}, Request{});
    });

    _server.onMessage([this](fluent::Context *context) {
        auto message = cast<std::tuple<Stream, Request>>(context->any);
        auto &stream = std::get<0>(message);
        auto &request = std::get<1>(message);
        Buffer &buffer = context->input;
        const char *begin = buffer.readBuffer();
        const char *end = buffer.readBuffer() + buffer.unread();
        const ssize_t parsed = _parser.parse(request, stream, begin, end);
        if(parsed < 0) {
            context->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            context->shutdown();
            return;
        }
        buffer.hasRead(parsed);
        if(stream.ready()) {
            const auto &connectionHeader = request.headers["Connection"];
            bool closeFlag = connectionHeader == "close"
                || (request.version == Request::Version::V10 && connectionHeader != "Keep-Alive");
            Response response;
            response.closeFlag = closeFlag;
            if(_requestCallback) {
                _requestCallback(&request, &response);
            }

            context->send(response.stream());

            if(response.closeFlag) {
                context->shutdown();
            }

            // reset
            request = Request{};
            stream = Stream{};
        }
    });
}

} // http
} // fluent
#endif