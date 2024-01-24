#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <set>

class ThreadNames
{
    mutable std::mutex mux_;
    std::map<std::thread::id,std::string> names_;
public:
    void setName(std::string name)
    {
        std::unique_lock<std::mutex> lock{mux_};
        names_[std::this_thread::get_id()] = std::move(name);
    }
    std::string getName(std::thread::id threadId)
    {
        std::unique_lock<std::mutex> lock{mux_};
        auto it = names_.find(threadId);
        return (names_.end() != it) ? it->second : (std::stringstream{} << threadId).str();
    }
    std::string getName()
    {
        return getName(std::this_thread::get_id());
    }
    std::map<std::thread::id,std::string> names() const
    {
        std::unique_lock<std::mutex> lock{mux_};
        return names_;
    }
};
template<typename T> class Singleton
{
public:
    static T & getInstance()
    {
        static T instance{};
        return instance;
    }
};
Singleton<ThreadNames> g_threadNames;
#define DBG_FUNC(NAME_STR) g_threadNames.getInstance().setName(NAME_STR);

namespace _utl {

    template<size_t SizeofItem> class ThreadSafeCircularAllocator {
    private:
        using TItem = char[SizeofItem];
        std::unique_ptr<TItem[]> buf_;
        uint32_t const capacity_;
        std::atomic_uint32_t availCapacity_, begin_, end_;
        std::atomic_flag isAllocating_;
        std::atomic_flag isDeallocating_;
    public:
        ThreadSafeCircularAllocator(uint32_t capacity)
            : buf_(new TItem[capacity])
            , capacity_(capacity)
            , availCapacity_(capacity)
            , end_(0)
            , begin_(0)
            , isAllocating_(false)
            , isDeallocating_(false)
        {
            //DBG std::memset(&buf_[0], 't', capacity);
        }
        
        /**
         * @brief Constructs a new Lockless Circular Allocator object
         * @pre The object is safe to move only right after its construction (before any method is called)
         * @param rhs Another object to move the data from
         */
        ThreadSafeCircularAllocator(ThreadSafeCircularAllocator && rhs)
            : buf_(std::move(rhs.buf_))
            , capacity_(rhs.capacity_)
            , availCapacity_(rhs.availCapacity_)
            , end_(rhs.end_)
            , begin_(rhs.begin_)
            , isAllocating_(true)
            , isDeallocating_(true)
        {
            while (rhs.isDeallocating_.test_and_set()) {}
            while (rhs.isAllocating_.test_and_set()) {}
            capacity_ = 0;
            availCapacity_.store(0);
            end_.store(0);
            begin_.store(0);
            rhs.isAllocating_.clear();
            rhs.isDeallocating_.clear();
        }
        ThreadSafeCircularAllocator(const ThreadSafeCircularAllocator &) = delete;

        ThreadSafeCircularAllocator & operator=(ThreadSafeCircularAllocator &&) = delete;
        ThreadSafeCircularAllocator & operator=(const ThreadSafeCircularAllocator &) = delete;

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
            return result;
        }
        void release(uint32_t length)
        {
            while (!tryDeallocate(length))
            {}
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
            assert(length);
            //DBG auto requestedLength = length;
            //DBG length += sizeof(uint32_t);
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
            //DBG std::fill((char*)result, (char*)result + length, '0');
            //DBG *static_cast<uint32_t*>(result) = requestedLength;
            //DBG return static_cast<uint32_t*>(result) + 1;
            return result;
        }
        bool tryDeallocate(uint32_t length)
        {
            //DBG auto requestedLength = length;
            //DBG length += sizeof(uint32_t);
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
                //DBG uint32_t actualLenghtToDeallocate = *reinterpret_cast<uint32_t*>(&buf_[beg % availCapacity_]);
                //DBG if (actualLenghtToDeallocate != requestedLength)
                //DBG {
                //DBG     uint32_t pos = 0;
                //DBG     std::cerr << "Blocks left to deallocate: " << std::endl;
                //DBG     while (pos != end)
                //DBG     {
                //DBG         uint32_t len = *reinterpret_cast<uint32_t*>(&buf_[pos]);
                //DBG         std::cerr << '[' << pos+4 << ".." << pos+len+4 << ']' << std::endl;
                //DBG         pos += len + 4;
                //DBG     }
                //DBG     std::cerr << "Blocks left to deallocate: " << std::endl;
                //DBG }
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
        std::mutex mutex_;
    public:
        std::set<std::thread::id> acquirers_;
        std::set<std::thread::id> releasers_;
        std::string debugName_;
    public:
        ~SimpleAllocator()
        {
            std::cerr << "SimpleAllocator '" << debugName_ << "' was acquired by";
            for (std::thread::id id : acquirers_) std::cerr << " {" << g_threadNames.getInstance().getName(id) << "}";
            std::cerr << " and released by";
            for (std::thread::id id : releasers_) std::cerr << " {" << g_threadNames.getInstance().getName(id) << "}";
            std::cerr << std::endl;
        }
        SimpleAllocator(
            uint32_t length
            , std::string debugName
        )
            : capacity_(length)
            , length_(0)
            , debugName_(std::move(debugName))
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
                acquirers_.insert(std::this_thread::get_id());
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
        inline void release(uint32_t length)
        {
            while (length)
            {
                std::unique_lock<std::mutex> lock{mutex_};
                releasers_.insert(std::this_thread::get_id());
                if (segments_.size())
                {
                    Blk & seg = segments_.front();
                    assert(seg.size == length);

                    length_ -= seg.size;
                    length = 0;
                    delete[] seg.ptr;
                    segments_.pop_front();
                }
            }
        }
    };

} // _utl namespace