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

#include <utl/io/Writer.hpp>
#include <utl/io/Reader.hpp>

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

    struct Arg {
        union Value {
            int8_t    i8;   uint8_t   u8;
            int16_t   i16;  uint16_t  u16;
            int32_t   i32;  uint32_t  u32;
            int64_t   i64;  uint64_t  u64;
            float     f32;  double    f64;
            char      Char;
            ThreadId  Thread;
            TimePoint EpochNsec;
            const void * ArrayPointer;
        };
        enum class TypeID : uint8_t {
            TI_NONE      = 0,  __ISARRAY            = 1,
            TI_u8        = 2,  TI_arrayof_u8        = 3,
            TI_u16       = 4,  TI_arrayof_u16       = 5,
            TI_u32       = 6,  TI_arrayof_u32       = 7,
            TI_u64       = 8,  TI_arrayof_u64       = 9,
            TI_i8        = 10, TI_arrayof_i8        = 11,
            TI_i16       = 12, TI_arrayof_i16       = 13,
            TI_i32       = 14, TI_arrayof_i32       = 15,
            TI_i64       = 16, TI_arrayof_i64       = 17,
            TI_f32       = 18, TI_arrayof_f32       = 19,
            TI_f64       = 20, TI_arrayof_f64       = 21,
            TI_Char      = 22, TI_arrayof_Char      = 23,
            TI_Thread    = 24, TI_arrayof_Thread    = 25,
            TI_EpochNsec = 26, TI_arrayof_EpochNsec = 27,
            TI_UNKNOWN   = 28,
            __TYPE_ID_COUNT
        };
        static uint8_t typeSize(TypeID type) {
            static const uint8_t typeId2Size[(int) TypeID::__TYPE_ID_COUNT] = {
                0, 0, // TI_NONE
                sizeof(uint8_t)  , sizeof(uint8_t),
                sizeof(uint16_t) , sizeof(uint16_t),
                sizeof(uint32_t) , sizeof(uint32_t),
                sizeof(uint64_t) , sizeof(uint64_t),
                sizeof(int8_t)   , sizeof(int8_t),
                sizeof(int16_t)  , sizeof(int16_t),
                sizeof(int32_t)  , sizeof(int32_t),
                sizeof(int64_t)  , sizeof(int64_t),
                sizeof(float)    , sizeof(float),
                sizeof(double)   , sizeof(double),
                sizeof(char)     , sizeof(char),
                sizeof(ThreadId), sizeof(ThreadId),
                sizeof(TimePoint), sizeof(TimePoint),
            };
            if (unsigned(type) >= (unsigned) TypeID::__TYPE_ID_COUNT) {
                throw std::runtime_error{ "Arg::typeSize() was given an invalid type ID" };
            }
            return typeId2Size[(unsigned) type];
        }
        Value valueOrArray;
        TypeID type;
        uint32_t arrayLength;
    };
    inline Arg::TypeID operator|(Arg::TypeID a, Arg::TypeID b)
    {
        assert(a == Arg::TypeID::__ISARRAY || b == Arg::TypeID::__ISARRAY);
        return static_cast<Arg::TypeID>(int(a) | int(b));
    }
    inline Arg::TypeID operator&(Arg::TypeID a, Arg::TypeID b)
    {
        assert(a == Arg::TypeID::__ISARRAY || b == Arg::TypeID::__ISARRAY);
        return static_cast<Arg::TypeID>(int(a) & int(b));
    }

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

namespace internal {

    template<typename TItem> class LocklessCircularAllocator {
    private:
        std::atomic<uint32_t> m_actualSize;
        std::atomic<uint32_t> m_acquireIndex;
        std::atomic<uint32_t> m_releaseIndex;
        uint32_t const m_size;
        TItem * const m_buf;
    public:
        ~LocklessCircularAllocator() {
            delete[] m_buf;
        }
        LocklessCircularAllocator(uint32_t size)
            : m_buf(new TItem[size])
            , m_size(size)
            , m_actualSize(size)
            , m_acquireIndex(0)
            , m_releaseIndex(0)
        {}
        LocklessCircularAllocator(const LocklessCircularAllocator &) = delete;
        LocklessCircularAllocator(LocklessCircularAllocator &&) = delete;
        LocklessCircularAllocator & operator=(const LocklessCircularAllocator &) = delete;
        LocklessCircularAllocator & operator=(LocklessCircularAllocator &&) = delete;

        inline bool isEmpty() { return m_acquireIndex == m_releaseIndex; }
        inline TItem * acquire(uint32_t size)
        {
            TItem * result = nullptr;
            uint32_t acquireIndex_old = m_acquireIndex;
            for (;;)
            {
                register uint32_t releaseIndex = m_releaseIndex;
                register uint32_t acquireIndex_new = acquireIndex_old + size;
                if (acquireIndex_old < releaseIndex)
                {
                    if (acquireIndex_new >= releaseIndex) continue;
                    if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                    result = m_buf + acquireIndex_old; break;
                }
                else // releaseIndex <= acquireIndex_old
                {
                    if (acquireIndex_new < m_size) {
                        if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                        result = m_buf + acquireIndex_old; break;
                    } else {
                        m_actualSize.store(acquireIndex_old);
                        acquireIndex_new = size;
                        if (acquireIndex_new >= releaseIndex) continue;
                        if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                        result = m_buf; break;
                    }
                }
            }
            TItem *end = result + size;
            TItem *p = result;
            while (p < end) new(p++) TItem{}; \
            return result;
        }
        inline void release(uint32_t size)
        {
            uint32_t releaseIndex_old = m_releaseIndex;
            for (;;)
            {
                register uint32_t acquireIndex = m_acquireIndex;
                if (releaseIndex_old == acquireIndex) {
                    return;
                }
                register uint32_t releaseIndex_new = releaseIndex_old + size;
                if (releaseIndex_old <= acquireIndex)
                {
                    if (releaseIndex_new > acquireIndex) continue;
                    if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                    return;
                }
                else // acquireIndex < releaseIndex_old
                {
                    if (releaseIndex_new < m_actualSize) {
                        if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                        return;
                    } else {
                        releaseIndex_new = size;
                        if (releaseIndex_new > acquireIndex) continue;
                        if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                        return;
                    }
                }
            }
        }
    };

}

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

namespace {
    template<class T> struct TypeIDGetter {
        static constexpr Arg::TypeID value = Arg::TypeID::TI_UNKNOWN;
    };
    template<> struct TypeIDGetter<char>     { static constexpr Arg::TypeID value = Arg::TypeID::TI_Char; };
    template<> struct TypeIDGetter<float>    { static constexpr Arg::TypeID value = Arg::TypeID::TI_f32; };
    template<> struct TypeIDGetter<double>   { static constexpr Arg::TypeID value = Arg::TypeID::TI_f64; };
    template<> struct TypeIDGetter<uint8_t>  { static constexpr Arg::TypeID value = Arg::TypeID::TI_u8; };
    template<> struct TypeIDGetter<uint16_t> { static constexpr Arg::TypeID value = Arg::TypeID::TI_u16; };
    template<> struct TypeIDGetter<uint32_t> { static constexpr Arg::TypeID value = Arg::TypeID::TI_u32; };
    template<> struct TypeIDGetter<uint64_t> { static constexpr Arg::TypeID value = Arg::TypeID::TI_u64; };
    template<> struct TypeIDGetter<int8_t>   { static constexpr Arg::TypeID value = Arg::TypeID::TI_i8; };
    template<> struct TypeIDGetter<int16_t>  { static constexpr Arg::TypeID value = Arg::TypeID::TI_i16; };
    template<> struct TypeIDGetter<int32_t>  { static constexpr Arg::TypeID value = Arg::TypeID::TI_i32; };
    template<> struct TypeIDGetter<int64_t>  { static constexpr Arg::TypeID value = Arg::TypeID::TI_i64; };

    inline void logEvent_sfinae(Arg * arg, std::thread::id && a) {
        arg->type = Arg::TypeID::TI_Thread;
        std::memcpy(&arg->valueOrArray.Thread, &a, sizeof(a));
    }
    inline void logEvent_sfinae_pchar(Arg * arg, const char * p) {
        arg->type = static_cast<Arg::TypeID>((int)TypeIDGetter<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->valueOrArray.ArrayPointer = p;
        arg->arrayLength = std::strlen(p);
    }
    inline void logEvent_sfinae(Arg * arg, const char * p) {
        logEvent_sfinae_pchar(arg, p);
    }
    inline void logEvent_sfinae(Arg * arg, char * p) {
        logEvent_sfinae_pchar(arg, p);
    }
    template<class C, class D> inline void logEvent_sfinae(Arg * arg, std::chrono::time_point<C,D> && a) {
        arg->type = Arg::TypeID::TI_EpochNsec;
        arg->valueOrArray.EpochNsec = a.time_since_epoch().count();
    }
    template<size_t N> inline void logEvent_sfinae(Arg * arg, const char (&p)[N]) {
        arg->type = static_cast<Arg::TypeID>((int)TypeIDGetter<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->valueOrArray.ArrayPointer = &p[0];
        arg->arrayLength = N-1;
    }
    template<class T> inline void logEvent_sfinae(Arg * arg, T && a) {
        arg->type = TypeIDGetter<T>::value;
        std::memcpy(&arg->valueOrArray, &a, sizeof(T));
    }
    template<class T, class...Ts> inline void logEvent_sfinae(Arg * argBuf, T && fst, Ts &&... args) {
        logEvent_sfinae(argBuf++, std::forward<T&&>(fst));
        logEvent_sfinae(argBuf, std::forward<Ts&&>(args)...);
    }
    inline void logEvent_sfinae(Arg * argBuf) { argBuf->type = Arg::TypeID::TI_NONE; }
}
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
        logEvent_sfinae(argBuf, std::forward<Ts&&>(args)...);
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

