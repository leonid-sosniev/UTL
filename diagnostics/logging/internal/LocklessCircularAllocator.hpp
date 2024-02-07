#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <utl/diagnostics/logging/internal/DebuggingMacros.hpp>

namespace _utl {

    template<
        typename TItem,
        typename = typename std::enable_if<std::is_pod<TItem>::value>::type
    >
    class ThreadSafeCircularAllocator {
    private:
        std::unique_ptr<TItem[],void(*)(TItem*)> buf_;
        uint32_t const capacity_;
        volatile std::atomic_uint32_t availCapacity_, begin_, end_;
        volatile std::atomic_flag isAllocating_;
        volatile std::atomic_flag isDeallocating_;
        static std::unique_ptr<TItem[],void(*)(TItem*)> makeUnique(uint32_t length)
        {
            void(*deleter)(TItem*) = length ? [](TItem* p) { delete[] p; } : [](TItem*) {};
            TItem * buffer = length ? new TItem[length] : nullptr;
            return {buffer, deleter};
        }
    public:
        std::function<void()> onOverflowEvent;
#if defined(DEBUG)
        std::string debugName;
#endif
        inline bool containsPointer(const void * pointer)
        {
            auto ptr = static_cast<const TItem*>(pointer);
            auto base = buf_.get();
            auto end = buf_.get() + capacity_;
            bool yes = base <= ptr && ptr < end;
            return yes;
        }

        ThreadSafeCircularAllocator(TItem * buffer, uint32_t length, void(*bufferDeleter)(TItem*))
            : buf_(buffer, bufferDeleter)
            , capacity_(length)
            , availCapacity_(length)
            , begin_(0)
            , end_(0)
            , isAllocating_(false)
            , isDeallocating_(false)
            , debugAllocatedSize_(0)
        {
            //DBG std::memset(&buf_[0], 't', capacity);
        }
        ThreadSafeCircularAllocator(uint32_t capacity = 0)
            : buf_(makeUnique(capacity))
            , capacity_(capacity)
            , availCapacity_(capacity)
            , begin_(0)
            , end_(0)
            , isAllocating_(false)
            , isDeallocating_(false)
            , debugAllocatedSize_(0)
        {
            //DBG std::memset(&buf_[0], 't', capacity);
        }

        ThreadSafeCircularAllocator(ThreadSafeCircularAllocator &&) = delete;
        ThreadSafeCircularAllocator(const ThreadSafeCircularAllocator &) = delete;

        ThreadSafeCircularAllocator & operator=(ThreadSafeCircularAllocator &&) = delete;
        ThreadSafeCircularAllocator & operator=(const ThreadSafeCircularAllocator &) = delete;

        std::atomic_size_t debugAllocatedSize_;
        inline bool isEmpty() const
        {
            return begin_ == end_;
        }
        void * acquire(uint32_t length)
        {
            void * result;
            do
            {
                result = tryAllocate(length);
            }
            while (!result);
            debugAllocatedSize_ += length;
            return result;
        }
        void release(uint32_t length)
        {
            while (!tryDeallocate(length))
            {}
            debugAllocatedSize_ -= length;
        }
        #define TRY_MOVE_END(OLD_VAL, NEW_VAL, RESULT_INDEX) \
        { \
            while (isAllocating_.test_and_set()) {} \
            if (end_.compare_exchange_weak(OLD_VAL, NEW_VAL)) result = &buf_[RESULT_INDEX]; \
            isAllocating_.clear(); \
        }
        #define TRY_MOVE_BEGIN(OLD_VAL, NEW_VAL) \
        { \
            while (isDeallocating_.test_and_set()) {} \
            if (begin_.compare_exchange_weak(OLD_VAL, NEW_VAL)) result = true; \
            isDeallocating_.clear(); \
        }
        void * tryAllocate(uint32_t length)
        {
            if (!length)
            {
                return &buf_[capacity_];
            }
            void * result = nullptr;
            uint32_t beg = begin_;
            uint32_t end = end_;
            if (end < beg)
            {
                if (length < beg - end)
                {
                    TRY_MOVE_END(end, end + length, end);
                }
            }
            else
            // beg <= end
            if (length < capacity_ - end)
            {
                TRY_MOVE_END(end, end + length, end);
            }
            else
            if (length == capacity_ - end && beg > 0)
            {
                TRY_MOVE_END(end, 0, end);
            }
            else
            if (length < beg)
            {
                while (isAllocating_.test_and_set())
                {}
                availCapacity_.store(end);
                if (end_.compare_exchange_weak(end, length))
                {
                    result = &buf_[0];
                }
                isAllocating_.clear();
            }
            if (!result && onOverflowEvent) onOverflowEvent();
            return result;
        }
        bool tryDeallocate(uint32_t length)
        {
            if (!length)
            {
                return true;
            }
            bool result = false;
            uint32_t beg = begin_;
            uint32_t end = end_;
            if (beg <= end)
            {
                if (end - beg >= length) TRY_MOVE_BEGIN(beg, beg + length);
            }
            else
            // end < beg
            {
                uint32_t begNew = (beg + length) % availCapacity_;
                if (length <= availCapacity_ - (beg - end)) TRY_MOVE_BEGIN(beg, begNew);
            }
            return result;
        }
        #undef TRY_MOVE_BEGIN
        #undef TRY_MOVE_END
    };

    template<size_t SizeofItem> class SimpleAllocator {
    private:
        using TItem = uint8_t[SizeofItem];
        uint32_t length_;
        const uint32_t capacity_;
        struct Blk { TItem * ptr; intmax_t size; };
        std::list<Blk> segments_;
        mutable std::mutex mutex_;
    public:
#if defined(DEBUG)
        std::set<std::thread::id> acquirers_;
        std::set<std::thread::id> releasers_;
        std::string debugName;
#endif
    public:
        uint32_t debugLength() const
        {
            std::unique_lock<std::mutex> lock{mutex_};
            return length_;
        }
        ~SimpleAllocator()
        {
#if defined(DEBUG)
            std::cerr << "SimpleAllocator '" << debugName << "' was acquired by";
            for (std::thread::id id : acquirers_) std::cerr << " {" << DBG_THREAD_GET_NAME_BY_TID(id) << "}";
            std::cerr << " and released by";
            for (std::thread::id id : releasers_) std::cerr << " {" << DBG_THREAD_GET_NAME_BY_TID(id) << "}";
            std::cerr << std::endl;
#endif
        }
        SimpleAllocator(
            uint32_t length
            , std::string debugName
        )
            : capacity_(length)
            , length_(0)
        {}
        SimpleAllocator(SimpleAllocator &&) = delete;
        SimpleAllocator(const SimpleAllocator &) = delete;
        SimpleAllocator & operator=(SimpleAllocator &&) = delete;
        SimpleAllocator & operator=(const SimpleAllocator &) = delete;

        inline bool isEmpty() const
        {
            std::unique_lock<std::mutex> lock{mutex_};
            return length_ == 0;
        }
        inline void * acquire(uint32_t length)
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock{mutex_};
#if defined(DEBUG)
                acquirers_.insert(std::this_thread::get_id());
#endif
                if (length_ < capacity_ - length)
                {
                    auto p = new TItem[length];
                    segments_.push_back(Blk{p, length});
                    length_ += length;
                    return p;
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        }
        inline void release(uint32_t length, bool exact = true)
        {
            while (length)
            {
                std::unique_lock<std::mutex> lock{mutex_};
#if defined(DEBUG)
                releasers_.insert(std::this_thread::get_id());
#endif
                assert(segments_.size());
                {
                    if (exact && segments_.front().size != length)
                    {
                        throw std::logic_error{"Wrong segment size"};
                    }
                    length_ -= segments_.front().size;
                    length -= segments_.front().size;
                    delete[] segments_.front().ptr;
                    segments_.pop_front();
                }
            }
        }
    };

} // _utl namespace

#include <utl/tester.hpp>

TEST_CASE("ThreadSafeCircularAllocator 2 threads", "[validation][multithread]")
{
    struct Item {
        uintptr_t p;
        uintptr_t v;
    };
    _utl::ThreadSafeCircularAllocator<Item> alloc{65536};
    std::thread t1{
        [](_utl::ThreadSafeCircularAllocator<Item> * alloc) {
            for (size_t i = 0; i < 1'000'000; ++i) {
                size_t n = i % 2 ? 5 : 31;
                bool deallocated = false;
                auto t = std::chrono::steady_clock::now();
                do {
                    deallocated = alloc->tryDeallocate(n);
                }
                while (!deallocated && (std::chrono::steady_clock::now() - t < std::chrono::seconds{1}));
                REQUIRE(deallocated);
            }
            REQUIRE(alloc->isEmpty());
        }, &alloc
    };
    std::thread t2{
        [](_utl::ThreadSafeCircularAllocator<Item> * alloc) {
            for (size_t i = 0; i < 1'000'000; ++i) {
                size_t n = i % 2 ? 5 : 31;
                void * ptr = nullptr;
                auto t = std::chrono::steady_clock::now();
                do {
                    ptr = alloc->tryAllocate(n);
                }
                while (!ptr && (std::chrono::steady_clock::now() - t < std::chrono::seconds{1}));
                REQUIRE(ptr);
            }
        }, &alloc
    };
    t1.join();
    t2.join();
}
