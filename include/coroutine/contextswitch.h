#pragma once
namespace fluent {
namespace co {

class Context;

// 可以安全地在gcc8及以上版本使用
// 否则需要避免-O0
//
// 由于不确定C++ ABI有啥坑，这里还是老实用上extern "C"
// 关于attribute：
// 1. 必须要no inline，保证rdi和rsi传递，否则很容易代码层面地inline（导致rsi没传递）
// 2. weak保证C++ inline作用，既weak符号，用于header-only库
// 3. optimize("O3")相当于即使你用O0编译整个程序，这一段代码也会用O3去编，
//    因为O0会生成多余的push/pop指令导致crash
extern "C" __attribute__((__noinline__, weak, optimize("O3")))
void contextSwitch(Context* prev /*%rdi*/, Context *next /*%rsi*/) {
    asm volatile(R"(
        movq %rsp, %rax
        movq %rax, 104(%rdi)
        movq %rbx, 96(%rdi)
        movq %rcx, 88(%rdi)
        movq %rdx, 80(%rdi)
        movq 0(%rax), %rax
        movq %rax, 72(%rdi)
        movq %rsi, 64(%rdi)
        movq %rdi, 56(%rdi)
        movq %rbp, 48(%rdi)
        movq %r8, 40(%rdi)
        movq %r9, 32(%rdi)
        movq %r12, 24(%rdi)
        movq %r13, 16(%rdi)
        movq %r14, 8(%rdi)
        movq %r15, (%rdi)

        movq 48(%rsi), %rbp
        movq 104(%rsi), %rsp
        movq (%rsi), %r15
        movq 8(%rsi), %r14
        movq 16(%rsi), %r13
        movq 24(%rsi), %r12
        movq 32(%rsi), %r9
        movq 40(%rsi), %r8
        movq 56(%rsi), %rdi
        movq 72(%rsi), %rax
        movq 80(%rsi), %rdx
        movq 88(%rsi), %rcx
        movq 96(%rsi), %rbx

        movq 64(%rsi), %rsi

        movq %rax, (%rsp)
        xorq %rax, %rax
    )");
    // 这里由gcc直接生成ret指令
}

} // co
} // fluent
