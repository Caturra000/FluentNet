#ifndef __MUTTY_FUTURE_FUNCTION_TRAITS_H__
#define __MUTTY_FUTURE_FUNCTION_TRAITS_H__
#include <bits/stdc++.h>
namespace mutty {

template <typename T>
struct FunctionTraits;

template <typename Ret, typename ...Args>
struct FunctionTraits<Ret(Args...)> {
    constexpr static size_t ArgsSize = sizeof...(Args);
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
};

template <typename Ret, typename ...Args>
struct FunctionTraits<Ret(*)(Args...)>: public FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename ...Args>
struct FunctionTraits<std::function<Ret(Args...)>>: public FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename C, typename ...Args>
struct FunctionTraits<Ret(C::*)(Args...)>: public FunctionTraits<Ret(Args...)> {};

template <typename Ret, typename C, typename ...Args>
struct FunctionTraits<Ret(C::*)(Args...) const>: public FunctionTraits<Ret(Args...)> {};

template <typename Callable>
struct FunctionTraits: public FunctionTraits<decltype(&Callable::operator())> {};

} // mutty
#endif