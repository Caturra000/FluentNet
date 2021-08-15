#ifndef __MUTTY_THROWS_ERRNO_EXCEPTION_H__
#define __MUTTY_THROWS_ERRNO_EXCEPTION_H__
#include <cstring>
#include "MuttyException.h"
namespace mutty {

class ErrnoException: public MuttyException {
private:
    int _err {0}; // 暂存errno
    std::string _what;
public:
    static constexpr const char *TAG = "errno exception";
    using MuttyException::MuttyException;
    ErrnoException(const char *info, int err)
        : MuttyException(info), _err(err), _what(std::string(info) + ' ' + strerror(_err)) {}
    ErrnoException(int err): ErrnoException(TAG, err) {}
    int errorCode() { return _err; }
    const char* errorMessage() { return strerror(_err); }
    const char* what() const noexcept override { return _what.c_str(); }
};

} // mutty
#endif