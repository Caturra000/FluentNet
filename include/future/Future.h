#ifndef __FLUENT_FUTURE_FUTURE_H__
#define __FLUENT_FUTURE_FUTURE_H__
#include <bits/stdc++.h>
#include "FunctionTraits.h"
#include "FutureTraits.h"
#include "Looper.h"
#include "ControlBlock.h"
#include "Promise.h"
namespace fluent {

// for SFINAE
namespace future_requires {

template <typename T, typename Functor>
struct Then:
    std::enable_if<
        IsThenValid<Future<T>, Functor>::value
    > {};

template <typename T, typename Functor>
struct Poll:
    std::enable_if<
        IsThenValid<Future<T>, Functor>::value &&
        std::is_same<typename FunctionTraits<Functor>::ReturnType, bool>::value
    > {};

template <typename T, typename Functor>
struct CancelIf:
    Poll<T, Functor> {};

template <typename T, typename Functor>
struct Wait:
    std::enable_if<
        IsThenValid<Future<T>, Functor>::value &&
        !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value &&
        std::is_same<typename FunctionTraits<Functor>::ReturnType, void>::value
    > {};
}

template <typename T>
class Promise;

template <typename T>
class Future {
// basic
public:
    Future(Looper *looper, SharedPtr<ControlBlock<T>> shared);
    Future(const Future &) = delete;
    Future(Future &&) = default;
    Future& operator=(const Future&) = delete;
    Future& operator=(Future&&) = default;

// asynchronous operations
public:
    // must receive functor R(T) R(T&) R(T&&)
    template <typename Functor,
              typename R = typename FunctionTraits<Functor>::ReturnType,
              typename = typename future_requires::Then<T, Functor>::type>
    Future<R> then(Functor &&f);

    // receive: bool(T) bool(T&) bool(T&&)
    // note: bool(T&&) may be unsafe (will move last future result)
    // return: Future<T>
    template <typename Functor,
              typename = typename future_requires::Poll<T, Functor>::type>
    Future<T> poll(Functor &&f);

    // receive: bool(T) bool(T&) bool(T&&)
    // note: bool(T&&) may be unsafe (will move last future result)
    // return: Future<T>
    template <typename Functor,
              typename = typename future_requires::Poll<T, Functor>::type>
    Future<T> poll(size_t count, Functor &&f);

    // receive: bool(T) bool(T&) bool(T&&)
    // return: Future<T>
    // note: bool(T&&) may be unsafe (will move last future result)
    // IMPROVEMENT: return future<T>& ?
    template <typename Functor,
              typename = typename future_requires::CancelIf<T, Functor>::type>
    Future<T> cancelIf(Functor &&f);

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              typename = typename future_requires::Wait<T, Functor>::type>
    Future<T> wait(size_t count, std::chrono::milliseconds duration, Functor &&f);

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              typename = typename future_requires::Wait<T, Functor>::type>
    Future<T> wait(std::chrono::milliseconds duration, Functor &&f);

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              typename = typename future_requires::Wait<T, Functor>::type>
    Future<T> wait(std::chrono::system_clock::time_point timePoint, Functor &&f);

// help for debug
public:
    bool hasResult();

    // !!UNSAFE
    // an unguarded function to get the value from the controlblock
    // and kill the shared state
    T get();

    // !!UNSAFE
    SharedPtr<ControlBlock<T>> getControlBlock();

private:
    // unsafe
    // ensure: READY & has then_
    void postRequest();

    // maintain the state machine
    // Callback: void(T&&, Promise<R>)
    template <typename R, typename Callback>
    Future<R> futureRoutine(Callback &&callback);

private:
    Looper                     *_looper;
    SharedPtr<ControlBlock<T>> _shared;
};

template <typename T>
inline Future<T>::Future(Looper *looper, SharedPtr<ControlBlock<T>> shared)
    : _looper(looper),
      _shared(std::move(shared)) {}

template <typename T>
template <typename Functor, typename R, typename>
inline Future<R> Future<T>::then(Functor &&f) {
    // T or T& or T&& ?
    // using ForwardType = decltype(std::get<0>(std::declval<typename FunctionTraits<Functor>::ArgsTuple>()));
    using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
    return futureRoutine<R>([f](T &&value, Promise<R> promise) {
        // for callback f
        // f(T) and f(T&) will copy current Future<T> value in shared,
        // - T will copy to your routine. It is safe to move
        // - T& just use reference where it is in shared state
        // T&& will move the parent future value
        // - T&& just use reference, but moving this value to your routine is unsafe
        promise.setValue(f(std::forward<ForwardType>(value)));
    });
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::poll(Functor &&f) {
    using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
    return futureRoutine<T>([f, looper = _looper](T &&value, Promise<T> promise) mutable {
        while(!f(std::forward<ForwardType>(value))) {
            looper->yield();
        }
        promise.setValue(std::forward<ForwardType>(value));
    });
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::poll(size_t count, Functor &&f) {
    using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
    return futureRoutine<T>([f, looper = _looper, count](T &&value, Promise<T> promise) mutable {
        while(count-- && !f(std::forward<ForwardType>(value))) {
            looper->yield();
        }
        promise.setValue(std::forward<ForwardType>(value));
    });
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::cancelIf(Functor &&f) {
    using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
    return futureRoutine<T>([f](T &&value, Promise<T> promise) {
        if(f(std::forward<ForwardType>(value))) {
            promise.cancel();
        } else {
            // forward the current future to the next
            promise.setValue(std::forward<ForwardType>(value));
        }
    });
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::wait(size_t count, std::chrono::milliseconds duration, Functor &&f) {
    auto start = std::chrono::system_clock::time_point{};
    return poll([f, start, duration, remain = count](T &self) mutable {
        auto current = std::chrono::system_clock::now();
        // call once
        if(start == std::chrono::system_clock::time_point{}) {
            // unlikely
            if(remain == 0) {
                return true;
            }
            start = current;
        }
        if(current - start >= duration) {
            f(self);
            if(--remain == 0) {
                return true;
            }
            start = /*current*/ std::chrono::system_clock::now();
        }
        return false;
    });
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::wait(std::chrono::milliseconds duration, Functor &&f) {
    return wait(1, duration, std::forward<Functor>(f));
}

template <typename T>
template <typename Functor, typename>
inline Future<T> Future<T>::wait(std::chrono::system_clock::time_point timePoint, Functor &&f) {
    return poll([f, timePoint](T &self) mutable {
        auto current = std::chrono::system_clock::now();
        if(current >= timePoint) {
            f(self);
            return true;
        }
        return false;
    });
}

template <typename T>
inline bool Future<T>::hasResult() {
    return _shared->_state == State::READY;
}

template <typename T>
inline T Future<T>::get() {
    auto value = std::move(_shared->_value);
    _shared->_state = State::DEAD;
    return value;
}

template <typename T>
inline SharedPtr<ControlBlock<T>> Future<T>::getControlBlock() {
    return _shared;
}

template <typename T>
inline void Future<T>::postRequest() {
    _shared->_state = State::POSTED;
    _looper->post([shared = _shared] {
        // shared->_value may be moved
        // _then must be T&&
        shared->_then(static_cast<T&&>(shared->_value));
        shared->_state = State::DONE;
    });
}

template <typename T>
template <typename R, typename Callback>
inline Future<R> Future<T>::futureRoutine(Callback &&callback) {
    Promise<R> promise(_looper);
    auto future = promise.get();
    State state = _shared->_state;
    if(state == State::NEW || state == State::READY) {
        // async request, will be set value and then callback

        // register request
        _shared->_then = [promise = std::move(promise), callback](T &&value) mutable {
            callback(std::forward<T>(value), std::move(promise));
        };

        // value is ready
        // post request immediately
        if(state == State::READY) {
            postRequest();
        }
    } else if(state == State::CANCEL) {
        // return a future will never be setValue()
        promise.cancel();
    }
    return future;
}

} // fluent
#endif