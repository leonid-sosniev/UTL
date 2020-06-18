#include <utl/threading/Spinlock.hpp>
#include <thread>

using namespace std::chrono;

_utl::SpinLock::SpinLock(bool useSleeping) : useSleeping(useSleeping)
{}

void _utl::SpinLock::lock()
{
    while (lck.test_and_set(std::memory_order_acquire))
    {
        if (useSleeping) { std::this_thread::sleep_for(milliseconds(0)); }
    }
}

bool _utl::SpinLock::try_lock_for(std::chrono::milliseconds msec)
{
    auto deadline = steady_clock::now() + msec;
    while (steady_clock::now() < deadline)
    {
        if (try_lock() == true)
        {
            return true;
        }
        else if (useSleeping) { std::this_thread::sleep_for(milliseconds(0)); }
    }
    return false;
}
bool _utl::SpinLock::try_lock() {
    return !lck.test_and_set(std::memory_order_acquire);
}
void _utl::SpinLock::unlock() {
    lck.clear(std::memory_order_release);
}

