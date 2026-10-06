#pragma once

#if defined(_WIN32)
#include <windows.h>
#endif
#include <chrono>
#include <thread>

namespace aether {

class FastMutex {
public:
    FastMutex() {
#if defined(_WIN32)
        InitializeCriticalSection(&cs_);
#endif
    }

    ~FastMutex() {
#if defined(_WIN32)
        DeleteCriticalSection(&cs_);
#endif
    }

    FastMutex(const FastMutex&) = delete;
    FastMutex& operator=(const FastMutex&) = delete;

    void lock() {
#if defined(_WIN32)
        EnterCriticalSection(&cs_);
#endif
    }

    void unlock() {
#if defined(_WIN32)
        LeaveCriticalSection(&cs_);
#endif
    }

private:
#if defined(_WIN32)
    CRITICAL_SECTION cs_;
#endif
};

class LockGuard {
public:
    explicit LockGuard(FastMutex& mtx) : mtx_(mtx) {
        mtx_.lock();
    }
    ~LockGuard() {
        mtx_.unlock();
    }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
private:
    FastMutex& mtx_;
};

inline void sleep_ms(int ms) {
#if defined(_WIN32)
    Sleep(static_cast<DWORD>(ms));
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

} // namespace aether
