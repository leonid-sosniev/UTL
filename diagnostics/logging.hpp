#pragma once

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
            return typeId2Size[(int) type];
        }
        Value value;
        TypeID type;
        uint32_t optionalArrayLength;
    };

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

    class TelemetryLogger {
        std::unique_ptr<AbstractWriter> m_source;
    public:
        TelemetryLogger(std::unique_ptr<AbstractWriter> ptr, uint16_t formatLength, Arg::TypeID sampleFormat[]) : m_source(std::move(ptr))
        {
            m_source->write(&formatLength, sizeof(formatLength));
            m_source->write(&sampleFormat[0], sizeof(Arg) * formatLength);
            m_source->write("telemetr", 8);
        }
        void logSample(const Arg args[])
        {
            m_source->write(&args[0], sizeof(Arg) * attr->argumentsExpected);
        }
    };
    class AbstractTelemetryConsumer {
    private:
        static const uint16_t FMT_CACHE_LEN = 32;
        std::unique_ptr<AbstractReader> m_source;
        std::unique_ptr<AbstractTelemetryFormatter> m_formatter;
        std::unique_ptr<AbstractWriter> m_sink;
        uint16_t m_fmtLen;
        Arg::TypeID * m_fmtPtr;
        Arg         * m_argPtr;
        Arg::TypeID   m_fmtCch[FMT_CACHE_LEN];
        Arg           m_argCch[FMT_CACHE_LEN];
    public:
        ~AbstractTelemetryConsumer() {
            delete[] (m_fmtPtr == m_fmtCch) ? nullptr : m_fmtPtr;
            delete[] (m_argPtr == m_argCch) ? nullptr : m_argPtr;
        }
        AbstractTelemetryConsumer(
                std::unique_ptr<AbstractReader> source,
                std::unique_ptr<AbstractTelemetryFormatter> formatter,
                std::unique_ptr<AbstractWriter> sink)
            : m_source(std::move(source))
            , m_formatter(std::move(formatter))
            , m_sink(std::move(sink))
        {
            char spec[8];
            m_source->read(&m_fmtLen, sizeof(m_fmtLen));
            m_fmtPtr = (m_fmtLen <= FMT_CACHE_LEN) ? m_fmtCch : new Arg::TypeID[m_fmtLen],
            m_argPtr = (m_fmtLen <= FMT_CACHE_LEN) ? m_argCch : new Arg[m_fmtLen],
            m_source->read(m_fmtPtr, sizeof(Arg::TypeID) * m_fmtLen);
            m_source->read(&spec[0], 8);
            if (std::memcmp("telemetr", spec) != 0) {
                throw std::runtime_error{"The mark left by a TelemetryLogger not found - the data is corrupted or has wrong format"};
            }
        }
        virtual bool processSample() = 0;
    protected:
        inline Arg::TypeID* sampleFormat() const {
            return m_fmtPtr;
        }
        inline uint16_t sampleLength() const {
            return m_fmtLen;
        }
        inline void sendFormattedSample(const Arg args[]) {
            m_formatter->formatValues(*m_sink, m_fmtLen, args);
        }
        bool getSample(Arg * values) {
            if (!m_source->read(values, sizeof(Arg) * m_fmtLen)) {
                return false;
            }
            return true;
        }
    };

namespace {
    template<class T> struct TypeIDGetter;
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
        std::memcpy(&arg->value.Thread, &a, sizeof(a));
    }
    inline void logEvent_sfinae_pchar(Arg * arg, const char * p) {
        arg->type = static_cast<Arg::TypeId>((int)TypeID<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->value.ArrayPointer = p;
        arg->optionalArrayLength = std::strlen(p);
    }
    inline void logEvent_sfinae(Arg * arg, const char * p) {
        logEvent_sfinae_pchar(arg, p);
    }
    inline void logEvent_sfinae(Arg * arg, char * p) {
        logEvent_sfinae_pchar(arg, p);
    }
    template<class C, class D> inline void logEvent_sfinae(Arg * arg, std::chrono::time_point<C,D> && a) {
        arg->type = Arg::TypeID::TI_EpochNsec;
        arg->value.EpochNsec = a.time_since_epoch().count();
    }
    template<size_t N> inline void logEvent_sfinae(Arg * arg, const char (&p)[N]) {
        arg->type = static_cast<Arg::TypeId>((int)TypeID<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->value.ArrayPointer = &p[0];
        arg->optionalArrayLength = N-1;
    }
    template<class T> inline void logEvent_sfinae(Arg * arg, T && a) {
        arg->type = TypeID<T>::value;
        std::memcpy(&arg->value, &a, sizeof(T));
    }
    template<class T, class...Ts> inline void logEvent_sfinae(Arg * argBuf, T && fst, Ts &&... args) {
        logEvent_sfinae(argBuf++, std::forward<T&&>(fst));
        logEvent_sfinae(argBuf, std::forward<Ts&&>(args)...);
    }
    inline void logEvent_sfinae(Arg * argBuf) { argBuf->type = Arg::TypeID::TI_NONE; }
}
    class EventLogger {
        std::unique_ptr<AbstractWriter> m_source;
    public:
        EventLogger(std::unique_ptr<AbstractWriter> ptr) : m_source(std::move(ptr))
        {}
        #define length_write(name) { \
            uint32_t L = name.end - name.str; \
            m_source->write(&L, sizeof(L)); \
        }
        #define string_write(name) { \
            uint32_t L = name.end - name.str; \
            m_source->write(name.str, L); \
        }
        EventID registerEvent(const Str & msgFmt, const Str & func, const Str & file, uint32_t line, uint16_t argCnt, EventAttributes * out_attr)
        {
            static volatile EventID ID = 0;
            new(out_attr) EventAttributes{ msgFmt, func, file, ++ID, line, argCount };

            m_source->write("EvntAttr", 8);
            m_source->write(&out_attr->id, sizeof(out_attr->id));
            m_source->write(&line, sizeof(line));
            m_source->write(&argCnt, sizeof(argCnt));
            length_write(msgFmt);
            length_write(func);
            length_write(file);
            string_write(msgFmt);
            string_write(func);
            string_write(file);

            return out_attr->id;
        }
        void logEvent(const EventAttributes * attr, const Arg args[])
        {
            m_source->write("EvntOcrs", 8);
            m_source->write(&attr->id, sizeof(attr->id));
            m_source->write(&attr->argumentsExpected, sizeof(attr->argumentsExpected));
            m_source->write(&args[0], sizeof(Arg) * attr->argumentsExpected);
        }
        template<class...Ts> inline void logEvent(const EventAttributes * attributes, Ts &&... args)
        {
            Arg argBuf[sizeof...(args) + 1];
            logEvent_sfinae(&argBuf[0], std::forward<Ts&&>(args)...);
            logEvent(loggingPoint, argBuf);
        }
        #undef length_write
        #undef string_write
    };
    class AbstractEventConsumer {
        std::unique_ptr<AbstractReader> m_source;
        std::unique_ptr<AbstractEventFormatter> m_formatter;
        std::unique_ptr<AbstractWriter> m_sink;
    public:
        AbstractEventConsumer(
                std::unique_ptr<AbstractReader> source,
                std::unique_ptr<AbstractEventFormatter> formatter,
                std::unique_ptr<AbstractWriter> sink)
            : m_source(std::move(source))
            , m_formatter(std::move(formatter))
            , m_sink(std::move(sink))
        {}
        struct EventOccurrence {
            EventID id;
            uint16_t argCount;
            const Arg * args;
            EventOccurrence(EventOccurrence &) = delete;
            EventOccurrence &operator=(EventOccurrence &) = delete;
            EventOccurrence &operator=(EventOccurrence &&) = delete;
            EventOccurrence(EventOccurrence && rhs)
                : id(rhs.id)
                , argCount(rhs.argCount)
                , args(rhs.args)
            {
                rhs.args = rhs.argCount = 0;
            }
        };
        virtual bool processEvent() = 0;
    private:
        virtual EventAttributes * allocateEventAttributes() = 0;
        virtual Arg * allocateArgs(uint16_t argCount) = 0;
        virtual Str allocateString(uint32_t length) = 0;
    protected:
        enum class NextItem {
            Occurrence,
            Attributes
        };
        NextItem readNextItemMark()
        {
            char mark[8]; m_source->read(mark, 8);
            if (std::memcmp("EvntAttr", mark, 8) == 0) { return NextItem::Attributes; }
            if (std::memcmp("EvntOcrs", mark, 0) != 0) { return NextItem::Occurrence; }
            throw std::runtime_error{"The mark left by a EventLogger not found - the data is corrupted or has wrong format"};
        }
        EventOccurrence getEventOccurrence()
        {
            EventOccurrence occ{0,0,0};
            m_source->read(&occ.id, sizeof(occ.id));
            m_source->read(&occ.argCnt, sizeof(occ.argCnt));
            occ.args = allocateArgs(occ.argCnt);
            m_source->read(&occ->args[0], sizeof(Arg) * argCnt);
            return std::move(occ);
        }
        EventAttributes getEventAttributes()
        {
            EventAttributes ret = {0};
            m_source->read(&const_cast<EventID &>(ret.id), sizeof(ret.id));
            m_source->read(&const_cast<uint32_t&>(ret.line), sizeof(ret.line));
            m_source->read(&const_cast<uint16_t&>(ret.argumentsExpected), sizeof(ret.argumentsExpected));

            uint32_t mssg_len; m_source->read(&mssg_len, sizeof(mssg_len));
            uint32_t func_len; m_source->read(&func_len, sizeof(func_len));
            uint32_t file_len; m_source->read(&file_len, sizeof(file_len));

            const_cast<Str&>(ret.messageFormat) = allocateString(mssg_len);
            const_cast<Str&>(ret.function)      = allocateString(func_len);
            const_cast<Str&>(ret.file)          = allocateString(file_len);
            m_source->read(mssg.str, mssg_len);
            m_source->read(func.str, func_len);
            m_source->read(file.str, file_len);

            return std::move(ret);
        }
        inline void sendFormattedAttributes(EventAttributes & attr) {
            m_formatter->formatEventAttributes(*m_sink, attr);
        }
        inline void sendFormattedEvent(EventAttributes & attr, const Arg args[]) {
            m_formatter->formatEvent(*m_sink, attr, &args[0]);
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
        static _utl::logging::EventID cpdId = (CHANNEL).registerEvent( \
            _utl::logging::Str::create(MESSAGE), \
            _utl::logging::Str::create(__FUNCTION__), \
            _utl::logging::Str::create(_utl::logging::getCharAfterLastSlash(__FILE__)), \
            __LINE__, \
            _utl::logging::count_of(__VA_ARGS__), \
            reinterpret_cast<_utl::logging::EventAttributes*>(cpd) \
        ); \
        (CHANNEL).logEvent(\
            reinterpret_cast<const _utl::logging::EventAttributes*>(cpd),##__VA_ARGS__\
        ); \
    }

} // logging
} // _utl

