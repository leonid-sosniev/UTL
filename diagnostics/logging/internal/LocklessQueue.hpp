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
        volatile std::atomic_flag flag_{false};
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

/*
    template<
        typename TItem
        >
    class ThreadSafeListBasedQueue
    {
        struct Chunk {
            TItem data;
            std::atomic<Chunk*> next;
        };
        struct List {
            std::atomic<Chunk*> head;
            std::atomic<Chunk*> tail;
        };
        const size_t capacity_;
        std::unique_ptr<Chunk[]> buffer_;
        List used_;
        List free_;
    private:
        Chunk * tryTakeChunk(List & list)
        {
            Chunk * p = list.head;
            if (p && list.head->compare_exchange_strong(p, p->next))
            {
                p->next = nullptr;
                return p;
            }
            return nullptr;
        }
        bool tryPutChunk(List & list, Chunk * chunk)
        {
            assert(chunk->next == nullptr);
            Chunk * p = nullptr;
            if (list.tail->next.compare_exchange_strong(p, chunk))
            {
                list.tail = chunk;
                return true;
            }
            return false;
        }
    public:
        ~ThreadSafeListBasedQueue()
        {}
        explicit ThreadSafeListBasedQueue(uint32_t capacity)
            : capacity_(std::max(capacity, 2))
            , buffer_(std::make_unique<Chunk[]>(capacity))
        {
            auto N = capacity_ - 1;
            for (size_t i = 0; i < N; ++i)
            {
                buffer_[i].next = &buffer_[i];
            }
            buffer_[N].next = nullptr;
            free_.head.store(&buffer[0]); used_.head.store(nullptr);
            free_.tail.store(&buffer[N]); used_.tail.store(nullptr);
        }
        bool tryDequeue(T & dest)
        {
            Chunk * taken = tryTakeChunk(used_);
            if (taken)
            {
                dest = taken->data;
                while (!tryPutChunk(free_, taken)) {}
                return true;
            }
            return false;
        }
        bool tryEnqueue(const T & item)
        {
            Chunk * freeChunk = tryTakeChunk(free_);
            if (freeChunk) {
                freeChunk->data = item;
                while (!tryPutChunk(used_, freeChunk)) {}
                return true;
            }
            return false;
        }
    };
*/

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
        volatile std::atomic<uint32_t> begin_;
        volatile std::atomic<uint32_t> end_;
        EnqueueLockStrategy encStrategy_;
        DequeueLockStrategy deqStrategy_;
    public:
#if defined(DEBUG)
        std::string debugName;
#endif
        std::function<void()> onOverflowEvent;
        ThreadSafeCircularQueue(uint32_t capacity)
            : items_(new TItem[capacity])
            , capacity_(capacity)
            , begin_(0)
            , end_(0)
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
        mutable std::mutex mutex_;
    public:
        BlockingQueue(uint32_t capacity = 0)
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
#if defined(DEBUG)
        std::string debugName;
#endif
    };

#ifdef CATCH_CONFIG_MAIN

#include <utl/tester.hpp>

TEST_CASE("0 ThreadSafeCircularQueue 2 threads", "[validation][multithread]")
{
    struct Item {
        uintptr_t p;
        uintptr_t v;
    };
    _utl::ThreadSafeCircularQueue<Item> alloc{1024};
    std::thread t1{
        [](_utl::ThreadSafeCircularQueue<Item> * alloc)
        {
            for (size_t i = 0; i < 1'000'000; ++i)
            {
                Item it{ i, i+1 };
                alloc->enqueue(std::move(it));
            }
        }, &alloc
    };
    std::thread t2{
        [](_utl::ThreadSafeCircularQueue<Item> * alloc)
        {
            for (size_t i = 0; i < 1'000'000; ++i)
            {
                Item it = alloc->dequeue();
                REQUIRE(it.p == i);
                REQUIRE(it.v == i+1);
            }
        }, &alloc
    };
    t1.join();
    t2.join();
}

#endif // CATCH_CONFIG_MAIN

} // namespace _utl