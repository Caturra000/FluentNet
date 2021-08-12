#ifndef __MUTTY_UITLS_DEFER_H__
#define __MUTTY_UITLS_DEFER_H__
#include <bits/stdc++.h>
namespace mutty {

#ifdef __GNUC__
class Defer: std::__shared_ptr<Defer, std::_S_single> {
#else
class Defer: std::shared_ptr<Defer> {
#endif
public:
    template <typename T, typename ...Args>
    Defer(T &&func, Args &&...args)
        : std::__shared_ptr<Defer, std::_S_single>
          (nullptr, [=](Defer*) { func(std::move(args)...); }) {}
};

} // mutty
#endif