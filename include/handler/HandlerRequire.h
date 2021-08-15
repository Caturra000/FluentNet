#ifndef __MUTTY_HANDLER_HANDLER_REQUIRE_H
#define __MUTTY_HANDLER_HANDLER_REQUIRE_H
#include <bits/stdc++.h>
namespace mutty {

template <typename Ret, typename ...Args>
struct HandlerRequire;

template <typename Ret, typename ...Args>
struct HandlerRequire<Ret(Args...)> {
    template <typename C>
    constexpr static void define(Ret (C::*fp)(Args...)) {}
};

} // mutty
#endif