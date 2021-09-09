# FluentNet

## TL;DR

`fluentNet`是一个基于C++14的TCP异步网络库，它有如下特点

- 完全异步：单线程也能异步，类似于`facebook/folly::future`的链式异步调用，轻松完成复杂业务
- 丰富扩展：仅需稍加代码，就能实现RPC等复杂应用（见examples）
- 高性能：异步+多线程+IO多路复用，懂的都懂，具体跑分见下方bench
- 异常安全：异步编程最头疼的异常安全问题，都帮你处理好了
- 配置简单：header only，迁移到项目连CMake都不需要

## 快速使用

以下是`echo`示例，你可以从中了解它的使用流程

Server端

```C++
#include <unistd.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "fluent.hpp"

int main() {
    // ip:port
    fluent::InetAddress address {INADDR_ANY, 2333};
    fluent::Server server {address};

    // 当接收到一条连接时
    server.onConnect([](fluent::Context *context) {
        context->socket.setNoDelay();
    });

    // 当收到消息时
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
```

Client端

```C++
int main(int argc, char *argv[]) {
    if(argc <= 1) {
        std::cerr << "usage: " << argv[0] << " host_ip" << std::endl;
        return -1;
    }
    using namespace std::chrono;
    using namespace std::chrono_literals;

    fluent::InetAddress address {argv[1], 2333};
    fluent::Client client;

    // 构造future，发出连接请求
    auto echo = client.connect(address)
        // 当请求成功后，可以执行then，与其他future的过程是异步的
        .then([](fluent::Context *context) {
            context->socket.setNoDelay();
            context->send("are you ok?\n");
            return context;
        })
        // 执行异步循环，只有true才会终止poll过程
        .poll([&client](fluent::Context *context) mutable {
            if(!context->isConnected()) {
                client.stop();
                return true;
            }
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
```

Server提供`callback`的形式，而Client主要使用`future`模型（当然两者都可以混用），下面会做更详细的说明

## Future

`fluentNet`最大的特点是支持`promise-future`模型，`future`本身是单线程的，因此不需要任何锁，但通过事件驱动仍可完全异步

### 构造

对于一个`typename T`类型的`fluent::Future<T>`

可通过两种方式来构造：

- 通过`fluent::Promise<T>::get()`结合`fluent::Promise<T>::setValue(T&&)`，用法与标准库`std::promise`一致
- 通过`fluent::makeFuture(Looper*, T&&)`直接构造处于就绪状态的`future`，在合适场合能减少异步调用次数

### 链式调用和异步请求

相比于标准库孱弱的feature，`fluent::Future`支持极其灵活的链式调用，可根据返回类型推导对应的`future`，如：

```C++
fluent::Looper looper;
std::map<std::string, std::vector<int>> string2vec; /*blabla*/

auto future = fluent::makeFuture(&looper, 19260817) // 如果构造的是一个int
    .then([](int val) { // 那么then就可以接收int / int& / int&&
        return std::to_string(val*2);
    })
    .then([&](std::string &&str) { // 同理，因为上一个链式调用返回`std::string`
        return string2vec[str]; // 那么可以推导出这里的lambda必须接收`std::string`或引用，否则不允许编译
    })
    .then([](std::vector<int> &vector) {
        return vector.size();
    }); // 最后推导出auto为fluent::Future<size_t>
```

（关于`T& / T&& / T`在行为上的细微区别暂不描述）

类型推导的目的是为了传递异步过程的上下文，每一次`then`都是异步请求，因此非常适合IO操作，也就是说，

future的最佳实践是：在`return`时发出对于IO的请求，获得关于这个IO的状态描述符，下一次`then`的调用时机已经是完成IO的时刻

比如构造一个虚拟的IO请求，这个请求得到响应大概需要500ms

```C++
struct IoDescriptor {
    std::chrono::system_clock::time_point _request;
    bool hasResponse() {
        using namespace std::chrono_literals;
        return std::chrono::system_clock::now() >= _request + 500ms;
    }
};

struct FakeIo {
    IoDescriptor makeRequest() {
        return IoDescriptor {std::chrono::system_clock::now()};
    }
};
```

那么我们可以这样，尝试发出100个请求

```C++
fluent::Looper looper;
FakeIo io;
for(size_t count = 100; count--;) {
    auto future = fluent::makeFuture(&looper, io.makeRequest())
        .then([](IoDescriptor request) {
            // 干你想干的事情...
        });
}
```

对于同步（不管是否阻塞）方法，完成所有请求并获得相应需要的时间是$100 * 500 ms$

而异步`future`基本上只需要$500ms$就能获得所有响应，因为请求和处理两个阶段是分离的，

但链式调用让它的形式近似于同步调用方法，你的异步业务逻辑不再是割裂的（回调地狱再见！）

## 更多异步语义

要逐一说明这些API的组合使用是个麻烦事情，可以参考内部代码实现（其实核心代码很短）以及`examples`

整个`fluentNet`的实现包括`future`本身也是靠`future`的异步语义组合完成的

对象函数

- `then`：接收函数签名`U(T / T& / T&&)`，`U`为任意返回类型，单次异步执行
- `poll`：接收函数签名`bool(T / T& / T&&)`，异步轮询，返回`true`表示获得结果，进行下一步，`false`表示失败，会再次发出相同的异步轮询，完成会传递当前异步上下文转发到下一个`future`
- `poll(count)`：同上，但至多允许失败`count`次
- `cancelIf`：接收函数签名`bool(T / T& / T&&)`，异步中断，如果返回`true`，下面的链式调用将不再进行，`false`则允许继续往下执行
- `wait(count)`：接收函数签名`void(T / T&)`，同步等待语义，将会调度`count`次执行函数对象
- `wait(std::chrono::milliseconds)`，同上，将会等待对应的时间段再回调
- `wait(std::chrono::system_clock::time_point timePoint)`，同上，只是形式换为时间点

类函数

- `whenAll`
- `whenN`
- `whenAny`
- `whenNIf`
- `whenAnyIf`

总的来说，这些接口可以做到

- 轮询
- 中断
- 同步（分为单`future`和多`future`，甚至不同模板类型的`future`）
- 谓语判断

这些丰富的语义能尽可能满足复杂的场合，配合`lambda`捕获，功能会非常强大

当然，不只是lambda，任意可调用对象都行

## Server

`fluent::Server`只需提供三个callback

- `onConnect`
- `onMesage`
- `onClose`

均接受`void(fluent::Context*)`的函数签名

如果你需要更高的性能，可以使用`fluent::BaseServer`，不同于`fluent::Server`的`std::function`实现，`fluent::BaseServer`是由用户提供回调函数的类型，从而避免不必要的虚函数开销，理论上支持任何可调用对象

## Client

`fluent::Client`同样支持上述`callback`，但更加推荐使用`future`

同样也有更高性能的`fluent::BaseClient`

## Benchmark

目前做了个简单的`ping-pong`基准测试来测量吞吐量，它并不能说明什么，只是衡量某种场合下大概的性能

作为比较的主流库有

- 作为未来标准的`boost::asio`
- 奇虎360基于`libevent`和`muduo`定制的`evpp`

测试的库均为最新，`g++`直接开`O3`，

每次`ping-pong`发出的消息大小固定为`16KB`

考虑到不同`client`实现会导致对`server`衡量的差异，这里`client`均采用`asio`实现

由于实现上的特殊性，`fluent`版本的`server`线程是指定的线程数减一，另一线程数用于全局共享的`poller`

而`fluent(multi)`版本是每个线程既处理`server`，也处理`poller`

表中结果的单位为`MiB/s`

| (threads / sessions) \\ network library | boost asio  | fluent      | fluent(multi) | qihoo360 evpp |
| --------------------------------------- | ----------- | ----------- | ------------- | ------------- |
| 2 / 10                                  | 2981.57     | 2555.91     | **3662.08**   | 3264.58       |
| 2 / 100                                 | 2467.58     | 1652.08     | **2647.6**    | 2090.98       |
| 2 /  1000                               | **1829.49** | 1320.73     | 1744.43       | 1585.79       |
| 4 / 10                                  | 1079.29     | 2215.28     | **2717.56**   | 631.259       |
| 4 / 100                                 | 2948.08     | **3904.77** | 3533.86       | 2350.77       |
| 4 /  1000                               | 2009.47     | **2041.09** | 2030.62       | 1911.82       |
| 8 / 10                                  | 590.297     | 612.672     | 724.709       | **766.125**   |
| 8 / 100                                 | 2238        | 2739.66     | **2791.97**   | 2432.45       |
| 8 /  1000                               | **1877.09** | 1783.53     | 1865.18       | 1866.43       |

<del>（你看这个360就是逊啦）</del>

## 实现总结

TODO 暂时没啥好的介绍方式

简单来说有若干特点

- 提供完全异步的`future`链式调用接口，只要有同步非阻塞接口，都能转换为异步，单线程也可以，并且避免回调地狱
- 定时器直接用`Future`搭配`Looper`替代
- `client`同时支持`callback`和`future`，个人刚需
- 尝试尽可能去除所有虚函数调用，避免`callback`只能是`std::function`的额外开销（`BaseServer/BaseClient`）
- 尝试用一种被动的内存回收算法（`utils/WeakReference`和`networks/Pool`）
- 把所有复杂的状态转换放到`Handler`中
- `networks/Context`单个类管理所有`TCP`通信相关特性，同时用各种`policy`类隔离私有实现
- `Multiplexer`、`Looper`、`Server`的生命周期相比以前实现的轮子`mutty`是完全倒置的，至少`Server`的生命周期大于`Looper`，`Multiplexer`的生命周期可以是独立的，也可以在`Server`内部，`epoll`本身线程安全，库内部只需少量锁即可线程安全
- 生命周期（`Lifecycle`）与`Context`分离，不需要显式使用`std::shared_ptr/std::weak_ptr`来声明`Context`生命周期，也避免明确不需要生命周期保护时不必要的引用计数。在使用上，`callback`形式是自身安全的，但`future`仍需要`Lifecycle`
- 通过一种直接在队列中求偏移量的方式实现`Context`中的异步`send`（`handleWriteComplete`和`sendPolicy`）

## TODO

TODO