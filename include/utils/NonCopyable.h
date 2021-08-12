#ifndef __MUTTY_UTILS_NON_COPYABLE_H__
#define __MUTTY_UTILS_NON_COPYABLE_H__
namespace mutty {

class NonCopyable {
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};

} // mutty
#endif