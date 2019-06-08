#include <utl/threading/Spinlock.hpp>
#include <thread>

_utl::SpinLock::SpinLock(bool useSleeping)
    : m_useSleeping(useSleeping)
{}

void _utl::SpinLock::lock()
{
    while (lck.test_and_set(std::memory_order_acquire))
    {
        if (m_useSleeping) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool _utl::SpinLock::try_lock_for(std::chrono::milliseconds msec)
{
    using namespace std::chrono;

    auto deadline = steady_clock::now() + msec;
    while (steady_clock::now() < deadline)
    {
        if (try_lock() == true)
        {
            return true;
        }
        else if (m_useSleeping) {
            std::this_thread::sleep_for(microseconds(250));
        }
    }
    return false;
}
bool _utl::SpinLock::try_lock() {
    return !lck.test_and_set(std::memory_order_acquire);
}
void _utl::SpinLock::unlock() {
    lck.clear(std::memory_order_release);
}

