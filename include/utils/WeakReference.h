#ifndef __MUTTY_UTILS_WEAK_REFERENCE_H__
#define __MUTTY_UTILS_WEAK_REFERENCE_H__
#include <bits/stdc++.h>
namespace mutty {

// GC interface with CRTP style
// should implement reusable() / isResuable() / get()
template <typename T, size_t step = 2>
class WeakReference {
public:
    WeakReference(): _reusableIndex(0), _window{0, -1} {}

    void updateReusableIndex();

public:
    int _reusableIndex;
    struct { int left, right; } _window;
};

template <typename T, size_t step>
inline void WeakReference<T, step>::updateReusableIndex() {
    auto resetWindow = [this] {
        _window.left = 0;
        _window.right = -1;
    };
    if(_reusableIndex != 0) {
        for(int _ = 0; _ < step; ++_) {
            if(_window.left > _window.right) {
                if(static_cast<T*>(this)->isResuable(_window.left)) {
                    ++_window.right;
                } else {
                    ++_window.left;
                    ++_window.right;
                    if(_window.left == _reusableIndex) {
                        resetWindow();
                    }
                }
                continue;
            }
            // left/right < reuseIndex
            if(_window.right + 1 == _reusableIndex) {
                _reusableIndex = _window.left;
                resetWindow(); // close window
            } else if(static_cast<T*>(this)->isResuable(_window.right + 1)) {
                ++_window.right;
            } else {
                std::swap(static_cast<T*>(this)->get(++_window.right),
                            static_cast<T*>(this)->get(_window.left++));
            }
        }
    }
}

} // mutty
#endif