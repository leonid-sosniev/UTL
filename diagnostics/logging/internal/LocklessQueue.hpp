#pragma once

#include <cstdint>

#include <atomic>
#include <memory>
#include <type_traits>

namespace _utl {

    template<typename TItem>
    class LocklessQueue
    {
        static_assert(std::is_trivial<TItem>::value, "The given TItem must be trivial so that dequeueing operation is safe");
        std::unique_ptr<TItem[]> items_;
        const uint32_t capacity_;
        std::atomic<uint32_t> begin_;
        std::atomic<uint32_t> end_;
#if defined(DEBUG)
        std::string debugName_;
#endif
    public:
        LocklessQueue(uint32_t capacity, std::string debugName = "")
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
        bool tryEnqueue(TItem && item)
        {
            // places the given item onto items_[start_], moves end_ forward
            uint32_t begin = begin_;
            uint32_t end = end_;
            uint32_t newEnd = (end + 1) % capacity_;

            if (newEnd == begin) // the queue is full
            {
                return false;
            }
            else
            if (end_.compare_exchange_weak(end, newEnd)) // success
            {
                items_[end] = std::move(item);
                return true;
            }
            else
            {
                return false;
            }
        }
        bool tryDequeue(TItem & dest)
        {
            uint32_t begin = begin_;
            uint32_t end = end_;
            uint32_t newBegin = (begin + 1) % capacity_;
            
            if (begin == end) // the queue is empty
            {
                return false;
            }
            else
            if (begin_.compare_exchange_weak(begin, newBegin)) // success
            {
                return true;
            }
            else
            {
                return false;
            }
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