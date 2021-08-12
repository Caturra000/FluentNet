#ifndef __MUTTY_THROWS_MUTTY_EXCEPTION_H__
#define __MUTTY_THROWS_MUTTY_EXCEPTION_H__
#include <bits/stdc++.h>
namespace mutty {

class MuttyException: public std::exception {
private:
    const char *_info;
public:
    explicit MuttyException(const char *info)
        : _info(info) {}
    const char* what() const noexcept override
        { return _info; }
};

} // mutty
#endif