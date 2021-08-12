#ifndef __MUTTY_UTILS_TIMESTAMP_H__
#define __MUTTY_UTILS_TIMESTAMP_H__
#include <chrono>
namespace mutty {

// using std::chrono::system_clock;
using namespace std::literals::chrono_literals;

using Nanosecond = std::chrono::nanoseconds;
using Microsecond = std::chrono::microseconds;
using Millisecond = std::chrono::milliseconds;
using Second = std::chrono::seconds;
using Minute = std::chrono::minutes;
using Hour = std::chrono::hours;
using Timestamp = std::chrono::system_clock::time_point;

inline Timestamp now() { return std::chrono::system_clock::now(); }
inline Timestamp nowAfter(Nanosecond interval) { return now() + interval; }
inline Timestamp nowBefore(Nanosecond interval) { return now() - interval; }

} // mutty
#endif