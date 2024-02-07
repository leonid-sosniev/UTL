#pragma once

#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <utl/diagnostics/logging/args.hpp>
#include <utl/io/Writer.hpp>
#include <utl/io/Reader.hpp>

#include <utl/diagnostics/logging/internal/LocklessQueue.hpp>
#include <utl/diagnostics/logging/internal/LocklessCircularAllocator.hpp>

#define DBG(x)

namespace {

    template<typename T>
    //using CircularAllocator = _utl::ThreadSafeCircularAllocator<sizeof(T)>;
    using CircularAllocator = _utl::SimpleAllocator<sizeof(T)>;
    template<typename T>
    using Queue = _utl::ThreadSafeCircularQueue<T>;

    /// @brief Helper class to replace std::vector as std::vector<T>::reserve triggers c-tor of T
    /// @tparam T 
    template<typename T> class Array
    {
        using Item = uint8_t[sizeof(T)];
        T * begin_;
        T * end_;
        uint32_t capacity_;
    public:
        Array()
            : begin_(nullptr)
            , end_(nullptr)
            , capacity_(0)
        {
        }
        ~Array()
        {
            assert(end_ - begin_ == capacity_);
            for (auto p = begin_; p < end_; ++p)
            {
                p->T::~T();
                std::memset(p, '\0', sizeof(T));
            }
            delete[] (Item*) begin_;
            end_ = begin_ = nullptr;
        }
        void reserve(uint32_t capacity)
        {
            assert(!begin_);
            begin_ = end_ = (T*) new Item[capacity];
            capacity_ = capacity;
        }
        template<typename ...TArgs> void emplace_back(TArgs &&... args)
        {
            assert(end_ - begin_ < capacity_);
            new (end_) T{std::forward<TArgs &&>(args)...};
            ++end_;
        }
        T * begin() { return begin_; }
        T * end() { return end_; }
        T & operator[](size_t i) { return begin_[i]; }
    };

} // namespace internal

namespace _utl { namespace logging {

    using TimePoint = uint64_t;
    using ThreadId = uint32_t;
    using EventID = uint32_t;

    struct Str {
        const char * str;
        const char * end;
    public:
        template<size_t N> static inline constexpr Str create(const char(&text)[N]) {
            return {text,text+N};
        }
        static inline constexpr Str create(const char *text) {
            return { text, *text ? create(text+1).end : text};
        }
    };

    struct EventAttributes {
    private:
        static std::atomic<uint32_t> ID_COUNTER;
    public:
        static EventID getNewEventID() { return ID_COUNTER.fetch_add(1); }
        Str messageFormat, function, file;
        EventID id;
        uint32_t line;
        size_t argumentsExpected; // must be size_t or else getting it at compile-time fails
    };
    std::atomic<uint32_t> EventAttributes::ID_COUNTER{1};

    class MemoryResource {
    private:
        friend class Logger;
        struct Block {
            CircularAllocator<char> * formattedDataAllocator;
            const void * data;
            uint16_t size;
            uint16_t capacity;
        };
        Queue<Block> & writerDataQueue_;
        CircularAllocator<char> & formattedDataAllocator_;
        char * lastAllocatedPtr_;
        uint16_t lastAllocatedSize_;
        std::atomic_bool available_;
    public:
        MemoryResource(Queue<Block> & writerDataQueue, CircularAllocator<char> & formattedDataAllocator)
            : formattedDataAllocator_(formattedDataAllocator)
            , writerDataQueue_(writerDataQueue)
            , lastAllocatedPtr_(nullptr)
            , lastAllocatedSize_(0)
            , available_(true)
        {
        }
        char * allocate(uint16_t initialSize)
        {
            if (!available_)
            {
                throw std::logic_error{"The memory resource became unavailable"};
            }
            assert(lastAllocatedPtr_ == nullptr);
            lastAllocatedSize_ += initialSize;
            lastAllocatedPtr_ = static_cast<char *>(formattedDataAllocator_.acquire(initialSize));
            assert(lastAllocatedPtr_ != nullptr);
            return lastAllocatedPtr_;
        }
        void submitAllocated(uint16_t meaningfulDataSize)
        {
            assert(meaningfulDataSize <= lastAllocatedSize_);
            writerDataQueue_.enqueue(
                Block{&formattedDataAllocator_, lastAllocatedPtr_, meaningfulDataSize, lastAllocatedSize_}
            );
            lastAllocatedPtr_ = nullptr;
            lastAllocatedSize_ = 0;
        }
        void submitStaticConstantData(const void * data, uint16_t size)
        {
            writerDataQueue_.enqueue(
                Block{&formattedDataAllocator_, data, size, 0}
            );
        }
        std::string debugName_;
    };
    
    class AbstractEventFormatter {
    protected:
        virtual void formatEventAttributes_(MemoryResource & mem, const EventAttributes & attr) = 0;
        virtual void formatEvent_(MemoryResource & mem, const EventAttributes & attr, const Arg args[]) = 0;
    public:
        void formatEventAttributes(MemoryResource & mem, const EventAttributes & attr)
        {
            formatEventAttributes_(mem, attr);
        }
        void formatEvent(MemoryResource & mem, const EventAttributes & attr, const Arg args[])
        {
            formatEvent_(mem, attr, args);
        }
        virtual ~AbstractEventFormatter() {}
    };

    class Logger {
    public:
        std::string debugName_;
    private:
        struct Ev
        {
            const EventAttributes * attr;
            const Arg * args;
        };
        Queue<Ev> eventQueue_;
        CircularAllocator<Arg> argAllocator_;
        Array<CircularAllocator<char>> formattedDataAllocators_;
        Array<MemoryResource> mems_;
        Array<std::thread> threadsOfFormatters_;
        Queue<MemoryResource::Block> writerDataQueue_;
        AbstractEventFormatter * fmt_;
        AbstractWriter * wtr_;
        volatile std::atomic<int8_t> isStopRequested_;
        std::thread threadW_;
    private:
        void formatterLoop(MemoryResource * mem)
        {
            MemoryResource &mem_ = *mem;
            DBG_FUNC(mem->debugName_ + "_fmt")
            Ev ev{};
            while (true)
            {
                if (eventQueue_.tryDequeue(ev))
                {
                    try
                    {
                        assert(ev.attr);
                        if (ev.args)
                        {
                            fmt_->formatEvent(mem_, *ev.attr, ev.args);
                            argAllocator_.release(ev.attr->argumentsExpected);
                        }
                        else
                        {
                            fmt_->formatEventAttributes(mem_, *ev.attr);
                        }
                    }
                    catch (std::logic_error &)
                    {
                    }
                }
                else
                if (isStopRequested_)
                {
                    if (mem->available_)
                    {
                        mem->available_.store(false);
                    }
                    else break;
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            }
            std::cerr << DBG_FUNC_GET_NAME() << " has ended its work" << std::endl;
        }
        void writerLoop()
        {
            DBG_FUNC(debugName_ + "_writer");
            while (true)
            {
                MemoryResource::Block blk{};
                if (writerDataQueue_.tryDequeue(blk))
                {
                    wtr_->write(blk.data, blk.size);
                    if (blk.capacity)
                    {
                        blk.formattedDataAllocator->release(blk.capacity);
                    }
                }
                else if (!isStopRequested_)
                {
                    wtr_->flush();
                }
                else
                {
                    break;
                }
            }
            std::cerr << DBG_FUNC_GET_NAME() << " writer has ended its work" << std::endl;
        }
    public:
        Logger(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger &operator=(Logger &&) = delete;
        Logger(uint16_t formatterMaxNumber, AbstractEventFormatter & fmt, AbstractWriter & wtr, uint32_t argsBufferLength, uint32_t eventsBufferLength, uint32_t formattingBufferSize, uint32_t writtingQueueLength, std::string debugName)
            : fmt_(&fmt)
            , wtr_(&wtr)
            , eventQueue_(eventsBufferLength)
            , argAllocator_(argsBufferLength)
            , writerDataQueue_(writtingQueueLength)
            , isStopRequested_{false}
            , threadW_(&Logger::writerLoop, this)
            , debugName_(debugName)
        {
            DBG_FUNC(debugName_ + "_logger");
            formattedDataAllocators_.reserve(formatterMaxNumber);
            threadsOfFormatters_.reserve(formatterMaxNumber);
            mems_.reserve(formatterMaxNumber);
            for (size_t i = 0; i < formatterMaxNumber; ++i)
            {
                formattedDataAllocators_.emplace_back(formattingBufferSize);
                mems_.emplace_back(writerDataQueue_, formattedDataAllocators_[i]);
                mems_[i].debugName_ = debugName_ + std::to_string(i);
                threadsOfFormatters_.emplace_back(&Logger::formatterLoop, this, &mems_[i]);
            }
        }
        ~Logger()
        {
            std::cerr << DBG_FUNC_GET_NAME() << std::endl;
            for (auto &th : threadsOfFormatters_)
            {
                isStopRequested_.store(true);
                if (th.joinable())
                {
                    th.join();
                }
            }
            if (threadW_.joinable())
            {
                isStopRequested_.store(true);
                threadW_.join();
            }
        }
        Arg * allocateArgsBuffer(const EventAttributes & attr) {
            return static_cast<Arg *>(argAllocator_.acquire(attr.argumentsExpected));
        }
        void registerEventAttributes(const EventAttributes & attr) {
            eventQueue_.enqueue({&attr,nullptr});
        }
        void logEvent(const EventAttributes & attr, const Arg args[]) {
            eventQueue_.enqueue({&attr,args});
        }
    };

namespace {
    constexpr const char * getCharAfterLastSlash_searchEnd(const char * str) {
        return *str ? getCharAfterLastSlash_searchEnd(str+1) : str;
    }
    constexpr const char * getCharAfterLastSlash_searchLastSlash(const char * str, const char * strEnd) {
        return (strEnd <= str)
            ? str
            : (*strEnd == '/' || *strEnd == '\\') ? strEnd : getCharAfterLastSlash_searchLastSlash(str,strEnd-1);
    }
    constexpr const char * getCharAfterLastSlash(const char * str)
    {
        return getCharAfterLastSlash_searchLastSlash(
            str,
            getCharAfterLastSlash_searchLastSlash(
                str,
                getCharAfterLastSlash_searchEnd(str)
            )-1
        );
    }

    template<class...T> constexpr inline size_t count_of(T &&... items) { return sizeof...(items); }

    inline const EventID registerEvent(Logger & logger, const EventAttributes & attr)
    {
        logger.registerEventAttributes(attr);
        return attr.id;
    }
    inline void logEvent(Logger & logger, const EventAttributes & attributes)
    {
        assert(attributes.argumentsExpected == 0);
        logger.logEvent(attributes, nullptr);
    }
    template<class...Ts> inline void logEvent(Logger & logger, const EventAttributes & attributes, Ts &&... argsPack)
    {
        assert(attributes.argumentsExpected);
        Arg * args = logger.allocateArgsBuffer(attributes);
        _utl::logging::internal::fillArgsBuffer(args, std::forward<Ts&&>(argsPack)...);
        logger.logEvent(attributes, args);
    }
}
    #define UTL_LOG_EVENT_ID cpd.id
    #define UTL_logev(CHANNEL, MESSAGE, ...) { \
        static const _utl::logging::EventAttributes cpd{ \
            _utl::logging::Str::create(MESSAGE), \
            _utl::logging::Str::create(__FUNCTION__), \
            _utl::logging::Str::create(_utl::logging::getCharAfterLastSlash(__FILE__)), \
            _utl::logging::EventAttributes::getNewEventID(), \
            __LINE__, \
            _utl::logging::count_of(__VA_ARGS__) \
        }; \
        static const auto purposed_to_call_registerEvent_once = _utl::logging::registerEvent(CHANNEL,cpd); \
        _utl::logging::logEvent(CHANNEL,cpd,##__VA_ARGS__); \
    }

} // logging
} // _utl

