#ifndef __FLUENT_V3_FUTURE_CONTROL_BLOCK_H__
#define __FLUENT_V3_FUTURE_CONTROL_BLOCK_H__
#include <bits/stdc++.h>
namespace fluent {

enum class State {
    // newcomer, mengxin
    // it may have _then
    NEW,
    // value has been set
    // it may have _then
    READY,
    // then() has been posted as a request in loop
    POSTED,
    // request is done
    // value may be moved
    DONE,
    // called by future.get()
    // value is moved
    // this state cannot be reused
    // (sub)task will be break, but you should not .get() while using .then()
    DEAD,
    // (sub)task will be break
    CANCEL,
};

// shared
// it may be a simple object without shared_ptr (in looper)
template <typename T>
struct ControlBlock {
    State                    _state;
    T                        _value;
    // std::exception_ptr       _exception;
    std::function<void(T&&)> _then;

    ControlBlock(): _state(State::NEW) {}
};

} // fluent
#endif