#pragma once

#include <cstdint>

#include <atomic>
#include <memory>
#include <type_traits>

namespace _utl {

    class ThreadSafeQueueStrategy_LocklessSingleReaderSingleWriter
    {
    public:
        bool isAllowedToEnqueue() { return true; }
        void finishEnqueue() {}
        bool isAllowedToDequeue() { return true; }
        void finishDequeue() {}
    };

    class ThreadSafeQueueStrategy_ManyReadersManyWriters
    {
        std::atomic_flag isAdding_;
        std::atomic_flag isRemoving_;
    public:
        bool isAllowedToEnqueue()
        {
            bool old = isAdding_.test_and_set();
            return !old;
        }
        void finishEnqueue()
        {
            isAdding_.clear();
        }
        bool isAllowedToDequeue()
        {
            bool old = isRemoving_.test_and_set();
            return !old;
        }
        void finishDequeue()
        {
            isRemoving_.clear();
        }
    };

    template<typename TItem, typename Strategy = ThreadSafeQueueStrategy_ManyReadersManyWriters>
    class LocklessQueue
    {
        static_assert(std::is_trivial<TItem>::value, "The given TItem must be trivial so that dequeueing operation is safe");
        std::unique_ptr<TItem[]> items_;
        const uint32_t capacity_;
        std::atomic<uint32_t> begin_;
        std::atomic<uint32_t> end_;
        std::atomic_flag endGuard_;
        std::atomic_flag beginGuard_;
#if defined(DEBUG)
        std::string debugName_;
#endif
    public:
        LocklessQueue(uint32_t capacity, std::string debugName = "")
            : items_(new TItem[capacity])
            , capacity_(capacity)
            , begin_(0), beginGuard_(false)
            , end_(0), endGuard_(false)
#if defined(DEBUG)
            , debugName_(std::move(debugName))
#endif
        {
        }
        bool tryEnqueue(TItem && item)
        {
            // places the given item onto items_[start_], moves end_ forward
            uint32_t begin = begin_;
            uint32_t end = end_;
            uint32_t newEnd = (end + 1) % capacity_;
            bool result = false;

            if (newEnd != begin)
            // not full
            {
                if (endGuard_.test_and_set() == 0)
                // allowed to enqueue
                {
                    if (end_.compare_exchange_weak(end, newEnd))
                    // do enqueue
                    {
                        items_[end] = std::move(item);
                        result = true;
                    }
                    endGuard_.clear();
                }
                else
                {
                    return false;
                }
            }
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
                if (beginGuard_.test_and_set() == 0)
                // allowed to dequeue
                {
                    if (begin_.compare_exchange_weak(begin, newBegin))
                    // do dequeue
                    {
                        dest = std::move(items_[begin]);
                        result = true;
                    }
                    beginGuard_.clear();
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