#pragma once

#include <cstdint>

#include <mutex>
#include <atomic>
#include <memory>
#include <type_traits>

namespace _utl {

    class ThreadSafeQueueStrategy_NoLock
    {
    public:
        bool try_lock() noexcept { return true; }
        void unlock() {}
    };

    class ThreadSafeQueueStrategy_AtomicLock
    {
        std::atomic_flag flag_{false};
    public:
        bool try_lock() noexcept
        {
            bool old = flag_.test_and_set();
            return !old;
        }
        void unlock()
        {
            flag_.clear();
        }
    };

    template<
        typename TItem,
        typename EnqueueLockStrategy = ThreadSafeQueueStrategy_AtomicLock,
        typename DequeueLockStrategy = ThreadSafeQueueStrategy_AtomicLock
        >
    class ThreadSafeCircularQueue
    {
        static_assert(std::is_trivial<TItem>::value, "The given TItem must be trivial so that dequeueing operation is safe");
        std::unique_ptr<TItem[]> items_;
        const uint32_t capacity_;
        std::atomic<uint32_t> begin_;
        std::atomic<uint32_t> end_;
        EnqueueLockStrategy encStrategy_;
        DequeueLockStrategy deqStrategy_;
#if defined(DEBUG)
        std::string debugName_;
#endif
    public:
        std::function<void()> onOverflowEvent;
        ThreadSafeCircularQueue(uint32_t capacity, std::string debugName = "")
            : items_(new TItem[capacity])
            , capacity_(capacity)
            , begin_(0)
            , end_(0)
#if defined(DEBUG)
            , debugName_(std::move(debugName))
#endif
        {
        }
        bool isEmpty() const
        {
            return begin_ == end_;
        }
        bool tryEnqueue(const TItem & item)
        {
            // places the given item onto items_[start_], moves end_ forward
            uint32_t begin = begin_;
            uint32_t end = end_;
            uint32_t newEnd = (end + 1) % capacity_;
            bool result = false;

            if (newEnd != begin)
            // not full
            {
                if (encStrategy_.try_lock())
                {
                    items_[end] = item;
                    if (end_.compare_exchange_strong(end, newEnd))
                    // do enqueue
                    {
                        result = true;
                    }
                    encStrategy_.unlock();
                }
                else
                {
                    return false;
                }
            }
            if (!result && onOverflowEvent) onOverflowEvent();
            return result;
        }
        bool tryDequeue(TItem & dest)
        {
            uint32_t begin = begin_;
            uint32_t end = end_;
            uint32_t newBegin = (begin + 1) % capacity_;
            bool result = false;
            
            if (begin != end)
            // the queue is not empty
            {
                if (deqStrategy_.try_lock())
                // allowed to dequeue
                {
                    dest = items_[begin];
                    if (begin_.compare_exchange_strong(begin, newBegin))
                    // do dequeue
                    {
                        result = true;
                    }
                    deqStrategy_.unlock();
                }
            }
            return result;
        }
        inline void enqueue(TItem && item)
        {
            while (!tryEnqueue(std::forward<TItem &&>(item)))
            {
            }
        }
        inline TItem dequeue()
        {
            TItem item;
            while (!tryDequeue(item))
            {
            }
            return item;
        }
    };

    template<typename TItem>
    class BlockingQueue
    {
        std::list<TItem> items_;
        const uint32_t capacity_;
        std::mutex mutex_;
    public:
        BlockingQueue(uint32_t capacity, std::string debugName = "")
            : items_()
            , capacity_(capacity)
        {
        }
        bool isEmpty() const
        {
            std::unique_lock<std::mutex> lock{mutex_};
            return items_.empty();
        }
        bool tryEnqueue(TItem && item)
        {
            std::unique_lock<std::mutex> lock{mutex_};
            if (items_.size() < capacity_)
            {
                items_.emplace_back(std::move(item));
                return true;
            }
            return false;
        }
        bool tryDequeue(TItem & dest)
        {
            std::unique_lock<std::mutex> lock{mutex_};
            if (items_.size() > 0)
            {
                dest = std::move(items_.front());
                items_.pop_front();
                return true;
            }
            return false;
        }
        inline void enqueue(TItem && item)
        {
            while (!tryEnqueue(std::forward<TItem &&>(item)))
            {
            }
        }
        inline TItem dequeue()
        {
            TItem item;
            while (!tryDequeue(item))
            {
            }
            return item;
        }
    };

} // namespace _utl