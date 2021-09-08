#include "forward.hpp"

// args: host_ip threads per_thread_sessions message_size timeout
int main(int argc, const char *argv[]) {
    if(argc <= 5) {
        std::cerr << "usage: " << argv[0]
                  << " host_ip threads per_thread_sessions message_size timeout(s)"
                  << std::endl;
        return -1;
    }

    std::vector<std::thread> tasks;
    std::atomic<size_t> gMessageCount{};
    std::atomic<size_t> gBytesCount{};
    std::atomic<size_t> gActive{};
    // auto multiplexer = std::make_shared<fluent::Multiplexer>();

    fluent::InetAddress address {argv[1], 2533};
    const int threads = ::atoi(argv[2]);
    const int sessions = ::atoi(argv[3]);
    const int size = ::atoi(argv[4]);
    const int timeout = ::atoi(argv[5]);
    const auto duration = std::chrono::seconds(timeout);
    const std::string blocks(size, 'a');

    using namespace std::chrono;
    // using namespace std::chrono_literals;

    auto task = [&] {
        auto dummy = [](fluent::Context*) {};
        using Dummy = decltype(dummy);
        fluent::BaseClient<Dummy, Dummy, Dummy> client(/*multiplexer,*/ dummy, dummy, dummy);
        system_clock::time_point last;
        for(size_t session = 0, countdown = sessions; session < sessions; ++session) {
            client.connect(address, [&](fluent::Context *context) {
                    context->socket.setNoDelay();
                    gActive++;
                    last = system_clock::now();
                    context->send(blocks);
                    return context;
                })
                .poll([&](fluent::Context *context) mutable {
                    auto now = system_clock::now();
                    if(now - last >= duration) {
                        return true;
                    }
                    auto &buf = context->input;
                    if(buf.unread()) {
                        gMessageCount++;
                        gBytesCount += buf.unread();
                        buf.hasRead(buf.unread());
                        context->send(blocks);
                    }
                    return false;
                }).then([&](fluent::Context *context) {
                    context->shutdown();
                    gActive--;
                    if(!--countdown) client.stop();
                    if(gActive == 0) {
                        milliseconds delta = duration_cast<milliseconds>(system_clock::now() - last);
                        double diff = delta.count() / 1000.0;
                        std::cout << gMessageCount << std::endl;
                        std::cout << gBytesCount / 1e6 / diff << "MiB/s" << std::endl;
                    }
                    return nullptr;
                });
        }
        client.run();
    };

    for(int count = threads; count--; tasks.emplace_back(task));
    // for(;/*stop*/;) multiplexer->poll(std::chrono::milliseconds::zero());
    for(auto &&task : tasks) task.join();
}