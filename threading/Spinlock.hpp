#pragma once

#include <chrono>
#include <atomic>
#include <thread>

namespace _utl
{

    namespace details { namespace SpinLock {

        template<bool enable = true> struct OptionalSleep {
            static void go() {
                std::this_thread::sleep_for(std::chrono::milliseconds(0));
            }
        };
        template<> struct OptionalSleep<false> {
            static void go() {}
        };

    }}

    template<bool useSleeping = false> class SpinLock {
    public:
        SpinLock()
        {}
        void lock()
        {
            while (lck.test_and_set(std::memory_order_acquire)) {
                details::SpinLock::OptionalSleep<useSleeping>::go();
            }
        }
        bool try_lock()
        {
            return !lck.test_and_set(std::memory_order_acquire);
        }
        bool try_lock_for(std::chrono::milliseconds msec)
        {
            auto deadline = std::chrono::steady_clock::now() + msec;
            while (std::chrono::steady_clock::now() < deadline) {
                if (try_lock() == true) {
                    return true;
                }
                else details::SpinLock::OptionalSleep<useSleeping>::go();
            }
            return false;
        }
        void unlock()
        {
            lck.clear(std::memory_order_release);
        }
    private:
        volatile std::atomic_flag lck = ATOMIC_FLAG_INIT;
    };

}