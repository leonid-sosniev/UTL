#pragma once

#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

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

    #include <utl/diagnostics/logging/internal/LocklessCircularAllocator.hpp>

    template<typename T>
    using LocklessCircularAllocator = _utl::LocklessCircularAllocator<T>;

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
        const Str messageFormat, function, file;
        const EventID id;
        const uint32_t line;
        const size_t argumentsExpected; // must be size_t or else getting it at compile-time fails
    };
    std::atomic<uint32_t> EventAttributes::ID_COUNTER{1};

    class AbstractEventFormatter {
    public:
        virtual ~AbstractEventFormatter() {}
        virtual void formatEventAttributes(AbstractWriter & wtr, const EventAttributes & attr) = 0;
        virtual void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg arg[]) = 0;
    };
    class AbstractTelemetryFormatter {
    public:
        virtual ~AbstractTelemetryFormatter() {}
        virtual void formatExpectedTypes(AbstractWriter & wtr, uint16_t count, const Arg::TypeID types[]) = 0;
        virtual void formatValues(AbstractWriter & wtr, uint16_t count, const Arg arg[]) = 0;
    };

    class AbstractEventChannel {
    protected:
        AbstractEventFormatter & m_formatter;
        AbstractWriter & m_writer;
        internal::LocklessCircularAllocator<Arg> m_argsAllocator;
    public:
        AbstractEventChannel & operator=(const AbstractEventChannel &) = delete;
        AbstractEventChannel & operator=(AbstractEventChannel &&) = delete;
        AbstractEventChannel(const AbstractEventChannel &) = delete;
        AbstractEventChannel(AbstractEventChannel &&) = delete;
    public:
        AbstractEventChannel(AbstractEventFormatter & formatter, AbstractWriter & writer, uint32_t eventArgsBufferSize)
            : m_formatter(formatter)
            , m_writer(writer)
            , m_argsAllocator(eventArgsBufferSize)
        {}
        virtual ~AbstractEventChannel() {}
        virtual bool tryReceiveAndProcessEvent() = 0;
        inline void sendEventAttributes(const EventAttributes & attr) { sendEventAttributes_(attr); }
        inline void sendEventOccurrence(const EventAttributes & attr, const Arg args[]) { sendEventOccurrence_(attr, args); }
        Arg * allocateArgs(uint32_t count) { return m_argsAllocator.acquire(count); }
    protected:
        void releaseArgs(uint32_t count) { m_argsAllocator.release(count); }
    private:
        virtual void sendEventAttributes_(const EventAttributes & attr) = 0;
        virtual void sendEventOccurrence_(const EventAttributes & attr, const Arg args[]) = 0;
    };
    class AbstractTelemetryChannel {
    protected:
        AbstractTelemetryFormatter & m_formatter;
        AbstractWriter & m_sink;
        internal::LocklessCircularAllocator<Arg> m_argsAllocator;
    public:
        AbstractTelemetryChannel(AbstractTelemetryFormatter & formatter, AbstractWriter & sink, uint32_t eventArgsBufferSize)
            : m_formatter(formatter)
            , m_sink(sink)
            , m_argsAllocator(eventArgsBufferSize)
        {}
        virtual bool tryProcessSample() = 0;
        Arg * allocateArgs(uint32_t count) { return m_argsAllocator.acquire(count); }
    protected:
        void releaseArgs(uint32_t count) { m_argsAllocator.release(count); }
    public:
        void sendSampleTypes(uint16_t sampleLength, const Arg::TypeID sampleTypes[]) {
            sendSampleTypes_(sampleLength, sampleTypes);
        }
        void sendSample(const Arg values[]) {
            sendSample_(values);
        }
    private:
        virtual void sendSampleTypes_(uint16_t sampleLength, const Arg::TypeID sampleTypes[]) = 0;
        virtual void sendSample_(const Arg values[]) = 0;
    };

    template<typename TEventChannel>
    inline const EventID registerEvent(TEventChannel & channel, const EventAttributes & attr)
    {
        channel.AbstractEventChannel::sendEventAttributes(attr);
        return attr.id;
    }
    template<
        typename TEventChannel, class...Ts,
        typename = typename std::enable_if<std::is_base_of<AbstractEventChannel,TEventChannel>::value>::type
    >
    inline void logEvent(TEventChannel & channel, const EventAttributes & attributes, Ts &&... args)
    {
        Arg * argBuf = channel.AbstractEventChannel::allocateArgs(sizeof...(Ts));
        _utl::logging::internal::fillArgsBuffer(argBuf, std::forward<Ts&&>(args)...);
        channel.AbstractEventChannel::sendEventOccurrence(attributes, argBuf);
    }

    template<
        class TTelemetryChannel = AbstractTelemetryChannel,
        class = typename std::enable_if<std::is_base_of<AbstractTelemetryChannel,TTelemetryChannel>::value>::type
    >
    class TelemetryLogger {
        TTelemetryChannel & m_wtr;
        uint16_t m_argCount;
    public:
        TelemetryLogger(TTelemetryChannel & wtr, uint16_t formatLength, Arg::TypeID sampleFormat[])
            : m_wtr(wtr)
            , m_argCount(formatLength)
        {
            m_wtr.sendSampleTypes(formatLength, sampleFormat);
        }
        void logSample(const Arg args[])
        {
            m_wtr.sendSample(args);
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
        static auto purposed_to_call_registerEvent_once = registerEvent(CHANNEL,cpd); \
        _utl::logging::logEvent(CHANNEL,cpd,##__VA_ARGS__); \
    }

} // logging
} // _utl

