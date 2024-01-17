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

namespace _utl { namespace logging {

namespace internal {

    #include <utl/diagnostics/logging/internal/LocklessQueue.hpp>
    #include <utl/diagnostics/logging/internal/LocklessCircularAllocator.hpp>

    // The combination of LocklessCircularAllocator, SimpleAllocator and BlockingQueue is good
    // The combination of SimpleAllocator, SimpleAllocator and BlockingQueue is good

    template<typename T>
    using LocklessCircularAllocator =
        _utl::LocklessCircularAllocator<sizeof(T)>
        //_utl::SimpleAllocator<sizeof(T)>
        ;
    template<typename T>
    using CircularAllocator =
        //_utl::LocklessCircularAllocator<sizeof(T)> // sometimes hangs in ~Logger as the writerLoop cannot release from mem_.allocator_
        _utl::SimpleAllocator<sizeof(T)>
        ;

    template<typename T>
    using LocklessQueue =
        //_utl::LocklessQueue<T> // all the time it tries to release in order different from aquire one
        _utl::BlockingQueue<T>
        ;

} // internal namespace

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
            const void * data;
            uint16_t size;
            uint16_t capacity;
        };
        internal::LocklessQueue<Block> queue_;
        internal::CircularAllocator<char> allocator_;
        char * lastAllocatedPtr_;
        uint16_t lastAllocatedSize_;
    public:
        MemoryResource(uint32_t formattingBufferSize, uint32_t writtingQueueLength)
            : allocator_(formattingBufferSize)
            , queue_(writtingQueueLength, "mem-res")
            , lastAllocatedPtr_(nullptr)
            , lastAllocatedSize_(0)
        {
        }
        char * allocate(uint16_t initialSize)
        {
            assert(lastAllocatedPtr_ == nullptr);
            lastAllocatedSize_ += initialSize;
            lastAllocatedPtr_ = static_cast<char *>(allocator_.acquire(initialSize));
            return lastAllocatedPtr_;
        }
        void submitAllocated(uint16_t meaningfulDataSize)
        {
            assert(meaningfulDataSize <= lastAllocatedSize_);
            queue_.enqueue(
                Block{lastAllocatedPtr_, meaningfulDataSize, lastAllocatedSize_}
            );
            lastAllocatedPtr_ = nullptr;
            lastAllocatedSize_ = 0;
        }
        void submitStaticConstantData(const void * data, uint16_t size)
        {
            queue_.enqueue(
                Block{data, size, 0}
            );
        }
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
    private:
        struct Ev
        {
            const EventAttributes * attr;
            const Arg * args;
        };
        internal::LocklessQueue<Ev> eventQueue_;
        internal::LocklessCircularAllocator<Arg> argAllocator_;
        MemoryResource mem_;
        AbstractEventFormatter * fmt_;
        AbstractWriter * wtr_;
        volatile std::atomic<int8_t> isActive_;
        const std::string debugName_;
        std::thread threadF_;
        std::thread threadW_;
    private:
        void deactivate() {
            isActive_.store(-1);
        }
        bool isActive() {
            return isActive_.load() > 0;
        }
        void formatterLoop()
        {
            Ev ev{};
            do
            {
                if (eventQueue_.tryDequeue(ev))
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
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
            }
            while (isActive());

            std::cerr << "FMT END" << debugName_ << std::endl;
        }
        void writerLoop()
        {
            while (true)
            {
                MemoryResource::Block blk{};
                if (mem_.queue_.tryDequeue(blk))
                {
                    wtr_->write(blk.data, blk.size);
                    if (blk.capacity)
                    {
                        mem_.allocator_.release(blk.capacity);
                    }
                }
                else if (isActive())
                {
                    wtr_->flush();
                }
                else
                {
                    std::cerr << "wtr end " << debugName_ << std::endl;
                    return;
                }
            }
        }
    public:
        Logger(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger &operator=(Logger &&) = delete;
        Logger(AbstractEventFormatter & fmt, AbstractWriter & wtr, uint32_t argsBufferLength, uint32_t eventsBufferLength, uint32_t formattingBufferSize, uint32_t writtingQueueLength, std::string debugName)
            : fmt_(&fmt)
            , wtr_(&wtr)
            , eventQueue_(eventsBufferLength, "log")
            , argAllocator_(argsBufferLength)
            , mem_(formattingBufferSize, writtingQueueLength)
            , threadF_(&Logger::formatterLoop, this)
            , threadW_(&Logger::writerLoop, this)
            , isActive_(1)
            , debugName_(std::move(debugName))
        {
        }
        ~Logger()
        {
            std::cerr << "Logger.dtor " << debugName_ << std::endl;
            deactivate();
            if (threadW_.joinable())
            {
                threadW_.join();
            }
            if (threadF_.joinable())
            {
                threadF_.join();
            }
            std::cerr << "Logger is gone " << debugName_ << std::endl;
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
    template<class...Ts> inline void logEvent(Logger & logger, const EventAttributes & attributes, Ts &&... argsPack)
    {
        Arg * args = logger.allocateArgsBuffer(attributes);
        _utl::logging::internal::fillArgsBuffer(args, std::forward<Ts&&>(argsPack)...);
        logger.logEvent(attributes, args);
    }
}
    #define UTL_logev(CHANNEL, MESSAGE, ...) { \
        static _utl::logging::EventAttributes cpd{ \
            _utl::logging::Str::create(MESSAGE), \
            _utl::logging::Str::create(__FUNCTION__), \
            _utl::logging::Str::create(_utl::logging::getCharAfterLastSlash(__FILE__)), \
            _utl::logging::EventAttributes::getNewEventID(), \
            __LINE__, \
            _utl::logging::count_of(__VA_ARGS__) \
        }; \
        static auto purposed_to_call_registerEvent_once = _utl::logging::registerEvent(CHANNEL,cpd); \
        _utl::logging::logEvent(CHANNEL,cpd,##__VA_ARGS__); \
    }

} // logging
} // _utl

