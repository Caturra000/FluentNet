#include <unistd.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../../fluent.hpp"

int main() {
    // shared multiplexer
    auto multiplexer = std::make_shared<fluent::Multiplexer>();
    std::vector<fluent::Server> servers;
    for(int off = 0; off < 3; ++off) {
        uint16_t port = 2335 + off;
        fluent::InetAddress address {INADDR_ANY, port};
        servers.emplace_back(address, multiplexer);
    }
    auto dummy = [](fluent::Context*) {};
    for(auto iter = servers.begin(); iter != servers.end(); ++iter) {
        auto pos = std::distance(servers.begin(), iter);
        iter->onConnect(dummy);
        iter->onClose(dummy);
        iter->onMessage([pos](fluent::Context *context) {
            char ch = 'A' + pos % 26;
            std::string str(5, ch);
            context->send(str + '\n');
        });
    }
    std::vector<std::thread> tasks;
    tasks.emplace_back([&] {
        for(;;) multiplexer->poll(std::chrono::milliseconds::zero());
    });
    for(auto &&server : servers) {
        auto &s = server;
        tasks.emplace_back([&] {
            s.ready();
            s.run();
        });
    }
    for(auto &&task : tasks) {
        task.join();
    }
}