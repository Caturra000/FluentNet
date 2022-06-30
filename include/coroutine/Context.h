#pragma once
#include <cstddef>
#include <cstring>
#include <iterator>
#include "contextswitch.h"

namespace fluent {
namespace co {

// low | regs[0]: r15  |
//     | regs[1]: r14  |
//     | regs[2]: r13  |
//     | regs[3]: r12  |
//     | regs[4]: r9   |
//     | regs[5]: r8   |
//     | regs[6]: rbp  |
//     | regs[7]: rdi  |
//     | regs[8]: rsi  |
//     | regs[9]: ret  |
//     | regs[10]: rdx |
//     | regs[11]: rcx |
//     | regs[12]: rbx |
// hig | regs[13]: rsp |

class Coroutine;

// 协程的上下文，只实现x86_64
class Context final {
public:
    using Callback = void(*)(Coroutine*);
    using Word = void*;

    constexpr static size_t STACK_SIZE = 1 << 17;
    constexpr static size_t RDI = 7;
    // constexpr static size_t RSI = 8;
    constexpr static size_t RET = 9;
    constexpr static size_t RSP = 13;

public:
    void prepare(Callback ret, Word rdi);

    void switchFrom(Context *previous);

    bool test();

private:
    Word getSp();

    void fillRegisters(Word sp, Callback ret, Word rdi, ...);

private:
    // 必须确保registers位于内存布局最顶端
    // 且不允许Context内有任何虚函数实现
    // 长度至少为14
    Word _registers[14];
    char _stack[STACK_SIZE];
};


inline void Context::switchFrom(Context *previous) {
    contextSwitch(previous, this);
}

inline void Context::prepare(Context::Callback ret, Context::Word rdi) {
    Word sp = getSp();
    fillRegisters(sp, ret, rdi);
}

inline bool Context::test() {
    char jojo;
    ptrdiff_t diff = std::distance(std::begin(_stack), &jojo);
    return diff >= 0 && diff < STACK_SIZE;
}

inline Context::Word Context::getSp() {
    auto sp = std::end(_stack) - sizeof(Word);
    sp = decltype(sp)(reinterpret_cast<size_t>(sp) & (~0xF));
    return sp;
}

inline void Context::fillRegisters(Word sp, Callback ret, Word rdi, ...) {
    ::memset(_registers, 0, sizeof _registers);
    auto pRet = (Word*)sp;
    *pRet = (Word)ret;
    _registers[RSP] = sp;
    _registers[RET] = *pRet;
    _registers[RDI] = rdi;
}


} // co
} // fluent
