#ifndef __FLUENT_FUTURE_LOOPER_H__
#define __FLUENT_FUTURE_LOOPER_H__

#define SKYWIND3000_CPU_LOOP_UNROLL_4X(actionx1, actionx2, actionx4, width) do { \
    unsigned long __width = (unsigned long)(width);    \
    unsigned long __increment = __width >> 2; \
    for (; __increment > 0; __increment--) { actionx4; }    \
    if (__width & 2) { actionx2; } \
    if (__width & 1) { actionx1; } \
}   while (0)

#define CATURRA_2X(action)  do { {action;} {action;} } while(0)
#define CATURRA_4X(action)  do { CATURRA_2X(action); CATURRA_2X(action); } while(0)
#define CATURRA_8X(action)  do { CATURRA_4X(action); CATURRA_4X(action); } while(0)
#define CATURRA_16X(action) do { CATURRA_8X(action); CATURRA_8X(action); } while(0)

#include <bits/stdc++.h>
#include "../coroutine/co.hpp"
namespace fluent {

class Looper {
    template <typename>
    friend class Future;

    template <typename>
    friend class Promise;

public:
    void loop();

    void loopOnce();

    // unsafe
    void loopOnceUnchecked();

    void yield();

    template <typename F, typename ...Args>
    void post(F &&f, Args &&...args);

private:
    void unroll4x();

private:
    using MessageQueue = std::queue<std::shared_ptr<co::Coroutine>>;
    co::Environment *_env {&co::open()};
    MessageQueue _mq;
};

inline void Looper::loop() {
    unroll4x();
}

inline void Looper::loopOnce() {
    if(!_mq.empty()) {
        loopOnceUnchecked();
    }
}

inline void Looper::loopOnceUnchecked() {
    auto co = std::move(_mq.front());
    _mq.pop();
    co->resume();
    if(co->running()) {
        _mq.emplace(std::move(co));
    }
}

inline void Looper::yield() {
    co::this_coroutine::yield();
}

template <typename F, typename ...Args>
inline void Looper::post(F &&f, Args &&...args) {
    _mq.emplace(_env->createCoroutine(
        std::forward<F>(f),
        std::forward<Args>(args)...));
}

inline void Looper::unroll4x() {
    size_t n = _mq.size();
    SKYWIND3000_CPU_LOOP_UNROLL_4X(
        {
            loopOnceUnchecked();
        },
        {
            CATURRA_2X(loopOnceUnchecked());
        },
        {
            CATURRA_4X(loopOnceUnchecked());
        },
        n
    );
}

} // fluent

#undef CATURRA_16X
#undef CATURRA_8X
#undef CATURRA_4X
#undef CATURRA_2X
#undef SKYWIND3000_CPU_LOOP_UNROLL_4X

#endif