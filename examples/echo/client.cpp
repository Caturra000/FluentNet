#include <unistd.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../../fluent.hpp"

int main(int argc, char *argv[]) {
    if(argc <= 1) {
        std::cout << "usage: " << argv[0] << " host_ip" << std::endl;
        return 0;
    }
    using namespace std::chrono;
    using namespace std::chrono_literals;

    fluent::InetAddress address {argv[1], 2333};
    fluent::Client client;

    auto echo = client.connect(address)
        .then([](fluent::Context *context) {
            context->socket.setNoDelay();
            context->send("are you ok?\n");
            return context;
        })
        .poll([last = system_clock::now(), &client](fluent::Context *context) mutable {
            if(!context->isConnected()) {
                std::cout << "what?" << std::endl;
                client.stop();
                return true;
            }
            auto now = system_clock::now();
            if(now - last < 500ms) {
                return false;
            }
            last = now;
            auto &buf = context->input;
            if(buf.unread()) {
                context->send(buf.readBuffer(), buf.unread());
                ::write(STDOUT_FILENO, buf.readBuffer(), buf.unread());
                buf.hasRead(buf.unread());
            };
            return false;
        });

    client.run();
}