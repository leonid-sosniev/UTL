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
        const Str messageFormat, function, file;
        const EventID id;
        const uint32_t line;
        const uint16_t argumentsExpected;
    };

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
    public:
        AbstractEventChannel & operator=(const AbstractEventChannel &) = delete;
        AbstractEventChannel & operator=(AbstractEventChannel &&) = delete;
        AbstractEventChannel(const AbstractEventChannel &) = delete;
        AbstractEventChannel(AbstractEventChannel &&) = delete;
    public:
        AbstractEventChannel(AbstractEventFormatter & formatter, AbstractWriter & writer)
            : m_formatter(formatter)
            , m_writer(writer)
        {}
        virtual ~AbstractEventChannel() {}
        virtual bool tryReceiveAndProcessEvent() = 0;
    private:
        template<class,class> friend class EventLogger;
        inline void sendEventAttributes(const EventAttributes & attr) { sendEventAttributes_(attr); }
        inline void sendEventOccurrence(const EventAttributes & attr, const Arg args[]) { sendEventOccurrence_(attr, args); }
        virtual void sendEventAttributes_(const EventAttributes & attr) = 0;
        virtual void sendEventOccurrence_(const EventAttributes & attr, const Arg args[]) = 0;
    };
    class AbstractTelemetryChannel {
    protected:
        AbstractTelemetryFormatter & m_formatter;
        AbstractWriter & m_sink;
    public:
        AbstractTelemetryChannel(AbstractTelemetryFormatter & formatter, AbstractWriter & sink)
            : m_formatter(formatter)
            , m_sink(sink)
        {}
        virtual bool tryProcessSample() = 0;
    private:
        template<class T1, class T2> friend class TelemetryLogger;
        void sendSampleTypes(uint16_t sampleLength, const Arg::TypeID sampleTypes[]) {
            sendSampleTypes_(sampleLength, sampleTypes);
        }
        void sendSample(const Arg values[]) {
            sendSample_(values);
        }
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
    template<
        class TEventChannel = AbstractEventChannel,
        class = typename std::enable_if<std::is_base_of<AbstractEventChannel,TEventChannel>::value>::type
    >
    class EventLogger {
        TEventChannel & m_wtr;
    public:
        EventLogger(TEventChannel & conduit) : m_wtr(conduit)
        {}
        EventID registerEvent(const Str & msgFmt, const Str & func, const Str & file, uint32_t line, uint16_t argCnt, EventAttributes * out_attr)
        {
            static volatile EventID ID = 0;
            new(out_attr) EventAttributes{ msgFmt, func, file, ++ID, line, argCnt };

            m_wtr.AbstractEventChannel::sendEventAttributes(*out_attr);
            return out_attr->id;
        }
        void logEvent(const EventAttributes & attr, const Arg args[])
        {
            m_wtr.AbstractEventChannel::sendEventOccurrence(attr, args);
        }
    };
    template<class Logger, class...Ts> inline void logEvent(Logger & logger, const EventAttributes & attributes, Ts &&... args)
    {
        Arg argBuf[sizeof...(args) + 1];
        logEvent_sfinae(&argBuf[0], std::forward<Ts&&>(args)...);
        logger.logEvent(attributes, argBuf);
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
        static uint8_t cpd[sizeof(_utl::logging::EventAttributes)]; \
        static _utl::logging::EventID purposed_to_call_registerEvent_once = (CHANNEL).registerEvent( \
            _utl::logging::Str::create(MESSAGE), \
            _utl::logging::Str::create(__FUNCTION__), \
            _utl::logging::Str::create(_utl::logging::getCharAfterLastSlash(__FILE__)), \
            __LINE__, \
            _utl::logging::count_of(__VA_ARGS__), \
            reinterpret_cast<_utl::logging::EventAttributes*>(cpd) \
        ); \
        logEvent(\
            (CHANNEL), *reinterpret_cast<const _utl::logging::EventAttributes*>(cpd),##__VA_ARGS__\
        ); \
    }

} // logging
} // _utl

