#ifndef __MUTTY_UTILS_COMPAT_H__
#define __MUTTY_UTILS_COMPAT_H__
#include <memory>
namespace mutty {
namespace cpp11 {

template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // cpp11
} // mutty
#endif