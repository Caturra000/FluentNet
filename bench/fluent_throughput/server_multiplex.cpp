#include "forward.hpp"

// args: threads
// threads > 1
int main(int argc, const char *argv[]) {
    if(argc <= 1) {
        std::cerr << "usage: " << argv[0] << " threads" << std::endl;
        return -1;
    }

    auto connectFunc = [](fluent::Context *context) {
        context->socket.setNoDelay();
    };
    auto messageFunc = [](fluent::Context *context) {
        auto &buf = context->input;
        if(buf.unread()) {
            context->send(buf.readBuffer(), buf.unread());
            buf.hasRead(buf.unread());
        }
    };
    auto dummyFunc = [](fluent::Context*) {};
    using ConnectFuncType = decltype(connectFunc);
    using MessageFuncType = decltype(messageFunc);
    using DummyFuncType = decltype(dummyFunc);

    using IServer = fluent::BaseServer<ConnectFuncType, MessageFuncType, DummyFuncType>;

    int threads = ::atoi(argv[1]);
    if(threads <= 1) {
        throw std::runtime_error("threads <= 1");
    }

    fluent::InetAddress address {INADDR_ANY, 2533};
    std::vector<IServer> servers;
    std::vector<std::thread> tasks;

    for(auto t = threads; t--;
        servers.emplace_back(address, connectFunc, messageFunc, dummyFunc));

    auto serverStart = [&tasks](IServer &server) {
        tasks.emplace_back([&server] {
            server.ready();
            server.run();
        });
    };
    auto threadJoin = [](std::thread &thread) {
        thread.join();
    };
    std::for_each(servers.begin(), servers.end(), serverStart);
    std::for_each(tasks.begin(), tasks.end(), threadJoin);
}