#include <unistd.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../../fluent.hpp"

// server: echo with max_connection_limit
// client: use telnet
int main(int argc, const char *argv[]) {
    if(argc < 2) {
        std::cout << "usage: " << argv[0] << " max_connection_limit" << std::endl;
        return 0;
    }
    fluent::InetAddress address {INADDR_ANY, 2334};
    fluent::Server server {address};

    int maxconnection = atoi(argv[1]);

    server.onConnect([&](fluent::Context *context) {
        if(maxconnection > 0) {
            maxconnection--;
            context->any = false;
        } else {
            context->shutdown();
            context->any = true;
        }
    });
    server.onMessage([](fluent::Context *context) {
        auto &buf = context->input;
        if(buf.unread()) {
            context->send(buf.readBuffer(), buf.unread());
            ::write(STDOUT_FILENO, buf.readBuffer(), buf.unread());
            buf.hasRead(buf.unread());
        };
    });
    server.onClose([&](fluent::Context *context) {
        if(!fluent::cast<bool>(context->any)) {
            maxconnection++;
        }
    });

    server.ready();
    server.run();
}