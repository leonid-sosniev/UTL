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
#include <deque>
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

#include <utl/diagnostics/logging/internal/DebuggingMacros.hpp>

namespace {

    template<typename T> using ArgAllocator = _utl::ThreadSafeCircularAllocator<T>;
    template<typename T> using FormatterAllocator = _utl::ThreadSafeCircularAllocator<T>;
    template<typename T> using EventQueue = _utl::ThreadSafeCircularQueue<T>;
    template<typename T> using WriterQueue = _utl::BlockingQueue<T>;

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

    struct Block {
        Block * next;
        const void * data;
        uint16_t size;
    };
    class MemoryResource;
    struct BlockList {
        MemoryResource * memoryResource;
        Block * firstBlock;
        uint32_t totalAllocatedSize;
        uint32_t totalAllocationsNumber;
    };

    class MemoryResource {
    private:
        friend class Logger;
        FormatterAllocator<char> & formattedDataAllocator_;
        WriterQueue<BlockList> & writerInputQueue_;
        Block * lastBlock_;
        BlockList pendingBlockList_;
        bool mustSubmit_;
    private:
        /// @brief This is to be invoked right after the event if completely formatted. Sends all formatted data blocks to writer.
        void friend_sendPendingBlocksToWriter()
        {
            if (mustSubmit_) {
                throw std::logic_error{"Bad formatter: Every allocate() call must be followed by a submitAllocated() call!"};
            }
            writerInputQueue_.enqueue(std::move(pendingBlockList_));
            pendingBlockList_.firstBlock = nullptr;
            pendingBlockList_.totalAllocatedSize = pendingBlockList_.totalAllocationsNumber = 0;
        }
        /// @brief This is to be invoked by writer. Deallocates all data of the last event, nullifies pointers to it.
        void friend_cleanupWrittenEventData(uint32_t totalAllocatedSize)
        {
            formattedDataAllocator_.release(totalAllocatedSize);
        }
    public:
        MemoryResource(WriterQueue<BlockList> & writerDataQueue, FormatterAllocator<char> & formattedDataAllocator)
            : formattedDataAllocator_(formattedDataAllocator)
            , writerInputQueue_(writerDataQueue)
            , lastBlock_(nullptr)
            , pendingBlockList_{this, nullptr, 0, 0}
            , mustSubmit_(false)
        {
            assert(pendingBlockList_.memoryResource == this);
        }
        char * allocate(uint16_t initialSize)
        {
            mustSubmit_ = true;
            auto dataBuff = formattedDataAllocator_.acquire(initialSize);
            assert(dataBuff);
            assert(pendingBlockList_.memoryResource == this);
            pendingBlockList_.totalAllocationsNumber += 1;
            pendingBlockList_.totalAllocatedSize += initialSize;
            return static_cast<char*>(dataBuff);
        }
        void submitAllocated(const void * data, uint16_t dataSize)
        {
            auto *segmBuff = allocate(sizeof(Block));
            auto *segm = new(segmBuff) Block{ nullptr, data, dataSize };
            if (pendingBlockList_.firstBlock) {
                assert(lastBlock_);
                assert(!lastBlock_->next);
                lastBlock_->next = segm;
            } else {
                pendingBlockList_.firstBlock = segm;
            }
            lastBlock_ = segm;
            mustSubmit_ = false;
        }
        void submitStaticConstantData(const void * data, uint16_t size)
        {
            submitAllocated(data, size);
        }
#if !defined(NDEBUG)
        std::string debugName;
#endif
    };

    constexpr size_t paddingTo(const size_t value, const size_t alignment) {
        return (value + (alignment - 1)) / alignment * alignment - value;
    }

    class WriterWorker
    {
        std::thread thread_;
    public:
        ~WriterWorker()
        {
            if (thread_.joinable())
            {
                thread_.join();
            }
        }
        WriterWorker(std::function<void()> threadMainFunction)
            : thread_(threadMainFunction)
        {
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
        // initialization order here matters much
        struct Event
        {
            const EventAttributes * attr;
            const Arg * args;
        };
        // stateless formatter objects and stateful writer objects
        AbstractEventFormatter * fmt_;
        AbstractWriter * wtr_;
        // inter-worker data channels
        ArgAllocator<Arg> argAllocator_;
        EventQueue<Event> eventQueue_;
        WriterQueue<BlockList> writerInputQueue_;
        /// must be set before formatterWorkers_ and writerWorker_
        volatile std::atomic_bool isFormattersStopRequested_;
        // must be destructed before formatterWorkers_
        WriterWorker writerWorker_;
        // must be initialized after fmt_, isFormattersStopRequested_
        std::deque<std::thread> formatterWorkers_;
    private:
        void formatterLoop(uint32_t formattedDataBufferSize, WriterQueue<BlockList> & writerInputQueue, uint16_t formatterIndex)
        {
            FormatterAllocator<char> formattedDataAllocator(formattedDataBufferSize);
            MemoryResource mem(writerInputQueue, formattedDataAllocator);
            try {
                Event ev{};
                while (true)
                {
                    if (isFormattersStopRequested_)
                    {
                        break;
                    }
                    else
                    if (eventQueue_.tryDequeue(ev))
                    {
                        try
                        {
                            assert(ev.attr);
                            if (ev.args)
                            {
                                fmt_->formatEvent(mem, *ev.attr, ev.args);
                                mem.friend_sendPendingBlocksToWriter();
                                argAllocator_.release(ev.attr->argumentsExpected);
                            }
                            else
                            {
                                fmt_->formatEventAttributes(mem, *ev.attr);
                            }
                        }
                        catch (std::logic_error &)
                        {
                        }
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds{1});
                    }
                }
            }
            catch (...)
            {
                std::cerr << "Formatter #" << formatterIndex << " has ended its work" << std::endl;
            }
        }
        void writerLoop()
        {
            while (true)
            {
                BlockList writableBlocks{};
                if (writerInputQueue_.tryDequeue(writableBlocks))
                {
                    auto count = writableBlocks.totalAllocationsNumber;
                    for (auto *blk = writableBlocks.firstBlock; blk; blk = blk->next)
                    {
                        assert(count);
                        wtr_->write(blk->data, blk->size);
                        count -= 1;
                    }
                    writableBlocks.memoryResource->friend_cleanupWrittenEventData(writableBlocks.totalAllocatedSize);
                }
                else
                if (isFormattersStopRequested_)
                {
                    break;
                }
                else
                {
                    wtr_->flush();
                }
            }
        }
    public:
        Logger(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger &operator=(Logger &&) = delete;
        Logger(uint16_t formatterMaxNumber, AbstractEventFormatter & fmt, AbstractWriter & wtr, uint32_t argsBufferLength, uint32_t eventsBufferLength, uint32_t formattingBufferSize, uint32_t writtingQueueLength)
            : fmt_(&fmt)
            , wtr_(&wtr)
            , argAllocator_(argsBufferLength)
            , eventQueue_(eventsBufferLength)
            , writerInputQueue_(writtingQueueLength)
            , isFormattersStopRequested_{false}
            , writerWorker_([this]() { writerLoop(); })
            , formatterWorkers_()
        {
            for (size_t i = 0; i < formatterMaxNumber; ++i)
            {
                formatterWorkers_.emplace_back(
                    &Logger::formatterLoop, this, formattingBufferSize, std::ref(writerInputQueue_), i
                );
            }
        }
        ~Logger()
        {
            std::cerr << DBG_THREAD_GET_NAME() << std::endl;
            isFormattersStopRequested_.store(true);
            for (auto & th : formatterWorkers_) {
                if (th.joinable()) {
                    th.join();
                }
            }
        }
        Arg * allocateArgsBuffer(const EventAttributes & attr) {
            auto p = static_cast<Arg *>(argAllocator_.acquire(attr.argumentsExpected));
            p[attr.argumentsExpected] = Arg{nullptr, Arg::TypeID::TI_NONE, 0};
            return p;
        }
        void registerEventAttributes(const EventAttributes & attr) {
            eventQueue_.enqueue({&attr,nullptr});
        }
        void logEvent(const EventAttributes & attr, const Arg args[]) {
            eventQueue_.enqueue({&attr,args});
        }
        bool isIdle() const {
            return eventQueue_.isEmpty() && writerInputQueue_.isEmpty();
        }
#if defined(DEBUG)
        std::string debugName;
#endif
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
        assert(sizeof...(Ts) == attributes.argumentsExpected);
        Arg * args = logger.allocateArgsBuffer(attributes);
        _utl::logging::internal::fillArgsBuffer(attributes.argumentsExpected, args, std::forward<Ts&&>(argsPack)...);
        logger.logEvent(attributes, args);
    }
} // namespace anonymous

    #define UNUSED(...)
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
        (void) purposed_to_call_registerEvent_once; \
        _utl::logging::logEvent(CHANNEL,cpd,##__VA_ARGS__); \
    }

} // logging
} // _utl

