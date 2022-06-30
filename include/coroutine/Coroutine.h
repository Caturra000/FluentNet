#pragma once
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include "State.h"
#include "Context.h"

namespace fluent {
namespace co {

class Environment;

class Coroutine: public std::enable_shared_from_this<Coroutine> {
    friend class Environment;
    friend class Context;

public:
    static Coroutine& current();

    // 测试当前控制流是否位于协程上下文
    static bool test();

    // 获取当前运行时信息
    // 目前仅支持运行、退出、主协程的判断
    const State runtime() const;
    bool exit() const;
    bool running() const;

    // 核心操作：resume和yield

    static void yield();

    // Note1: 允许处于EXIT状态的协程重入，从而再次resume
    //        如果不使用这种特性，则用exit() / running()判断
    //
    // Note2: 返回值可以得知resume并执行后的运行时状态
    //        但是这个值只适用于简单的场合
    //        如果接下来其它协程的运行也影响了该协程的状态
    //        那建议用runtime()获取
    const State resume();

    // usage: Coroutine::current().yield()
    // void yield();

    Coroutine(const Coroutine&) = delete;
    Coroutine(Coroutine&&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;
    Coroutine& operator=(Coroutine&&) = delete;

// 由于用到std::make_shared，必须公开这个构造函数
// TODO 设为private
public:

    // 构造Coroutine执行函数，entry为函数入口，对应传参为arguments...
    // Note: 不可重入
    template <typename Entry, typename ...Args>
    Coroutine(Environment *master, Entry &&entry, Args &&...arguments)
        : _entry([=] { entry(std::move(arguments)...); }),
          _master(master) {}

private:
    static void callWhenFinish(Coroutine *coroutine);

private:
    State _runtime {};
    Context _context;
    std::function<void()> _entry;
    Environment *_master;
};

class Environment {
    friend class Coroutine;
public:
    static Environment& instance();

    template <typename Entry, typename ...Args>
    std::shared_ptr<Coroutine> createCoroutine(Entry &&entry, Args &&...arguments);

    Coroutine* current();

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;

private:
    void push(std::shared_ptr<Coroutine> coroutine);
    void pop();
    Environment();

private:
    std::vector<std::shared_ptr<Coroutine>> _cStack;
    std::shared_ptr<Coroutine> _main;
};


template <typename Entry, typename ...Args>
inline std::shared_ptr<Coroutine> Environment::createCoroutine(Entry &&entry, Args &&...arguments) {
    return std::make_shared<Coroutine>(
        this, std::forward<Entry>(entry), std::forward<Args>(arguments)...);
}

inline Environment& Environment::instance() {
    static thread_local Environment env;
    return env;
}

inline Coroutine* Environment::current() {
    return _cStack.back().get();
}

inline void Environment::push(std::shared_ptr<Coroutine> coroutine) {
    _cStack.emplace_back(std::move(coroutine));
}

inline void Environment::pop() {
    _cStack.pop_back();
}

inline Environment::Environment() {
    _main = std::make_shared<Coroutine>(this, [](){});
    // TODO set State
    push(_main);
}



inline Coroutine& Coroutine::current() {
    return *Environment::instance().current();
}

inline bool Coroutine::test() {
    return current()._context.test();
}

inline const State Coroutine::runtime() const {
    return _runtime;
}

inline bool Coroutine::exit() const {
    return _runtime & State::EXIT;
}

inline bool Coroutine::running() const {
    return _runtime & State::RUNNING;
}

inline const State Coroutine::resume() {
    if(_runtime & State::EXIT) {
        return _runtime;
    }
    if(!(_runtime & State::RUNNING)) {
        _context.prepare(Coroutine::callWhenFinish, this);
        _runtime |= State::RUNNING;
    }
    auto previous = _master->current();
    _master->push(shared_from_this());
    _context.switchFrom(&previous->_context);
    return _runtime;
}

inline void Coroutine::yield() {
    auto &coroutine = current();
    auto &currentContext = coroutine._context;

    coroutine._master->pop();

    auto &previousContext = current()._context;
    previousContext.switchFrom(&currentContext);
}

inline void Coroutine::callWhenFinish(Coroutine *coroutine) {
    auto &routine = coroutine->_entry;
    auto &runtime = coroutine->_runtime;
    if(routine) routine();
    runtime ^= (State::EXIT | State::RUNNING);
    // coroutine->yield();
    yield();
}

} // co
} // fluent
