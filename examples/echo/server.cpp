#include <unistd.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../../fluent.hpp"

int main() {
    fluent::InetAddress address {INADDR_ANY, 2333};
    fluent::Server server {address};

    server.onConnect([](fluent::Context *context) {
        context->socket.setNoDelay();
    });

    server.onMessage([](fluent::Context *context) {
        auto &buf = context->input;
        if(buf.unread()) {
            context->send(buf.readBuffer(), buf.unread());
            ::write(STDOUT_FILENO, buf.readBuffer(), buf.unread());
            buf.hasRead(buf.unread());
        };
    });

    server.ready();
    server.run();
}