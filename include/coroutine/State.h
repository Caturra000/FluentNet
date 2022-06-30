#pragma once

namespace fluent {
namespace co {

// 协程的运行时状态位
struct State {
    using Bitmask = unsigned char;

    constexpr static Bitmask MAIN = 1 << 0;
    constexpr static Bitmask IDLE = 1 << 1;
    constexpr static Bitmask RUNNING = 1 << 2;
    constexpr static Bitmask EXIT = 1 << 3;

    Bitmask operator&(Bitmask mask) const { return flag & mask; }
    Bitmask operator|(Bitmask mask) const { return flag | mask; }
    Bitmask operator^(Bitmask mask) const { return flag ^ mask; }

    void operator&=(Bitmask mask) { flag &= mask; }
    void operator|=(Bitmask mask) { flag |= mask; }
    void operator^=(Bitmask mask) { flag ^= mask; }

    Bitmask flag;
};

} // co
} // fluent
