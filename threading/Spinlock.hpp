#pragma once

#include <chrono>
#include <atomic>

namespace _utl
{

    class SpinLock {
    public:
        SpinLock(bool useSleeping = false);
        void lock();
        bool try_lock();
        bool try_lock_for(std::chrono::milliseconds);
        void unlock();
    private:
        volatile std::atomic_flag lck = ATOMIC_FLAG_INIT;
        const bool useSleeping;
    };

}