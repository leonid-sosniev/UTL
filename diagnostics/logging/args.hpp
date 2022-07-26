#pragma once

#include <cassert>
#include <cstdint>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace _utl { namespace logging {

    using TimePoint = uint64_t;
    using ThreadId = uint32_t;

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
        static const char * typeName(TypeID type) {
            static const char * typeId2Name[(int) TypeID::__TYPE_ID_COUNT] = {
                "NONE"     , ""          ,
                "uint8_t"  , "uint8_t[]" ,
                "uint16_t" , "uint16_t[]",
                "uint32_t" , "uint32_t[]",
                "uint64_t" , "uint64_t[]",
                "int8_t"   , "int8_t[]"  ,
                "int16_t"  , "int16_t[]" ,
                "int32_t"  , "int32_t[]" ,
                "int64_t"  , "int64_t[]" ,
                "float"    , "float[]"   ,
                "double"   , "double[]"  ,
                "char"     , "char[]"    ,
                "ThreadId" , "ThreadId[]",
                "TimePoint", "TimePoint[]",
            };
            if (unsigned(type) >= (unsigned) TypeID::__TYPE_ID_COUNT) {
                throw std::runtime_error{ "Arg::typeName() was given an invalid type ID" };
            }
            return typeId2Name[(unsigned) type];
        }
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
    constexpr inline Arg::TypeID operator|(Arg::TypeID a, Arg::TypeID b)
    {
        assert(a == Arg::TypeID::__ISARRAY || b == Arg::TypeID::__ISARRAY);
        return static_cast<Arg::TypeID>(int(a) | int(b));
    }
    constexpr inline bool operator&(Arg::TypeID a, Arg::TypeID b)
    {
        assert(a == Arg::TypeID::__ISARRAY || b == Arg::TypeID::__ISARRAY);
        return int(a) & int(b);
    }

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
    template<> struct TypeIDGetter<std::thread::id> {
        static constexpr Arg::TypeID value = Arg::TypeID::TI_Thread;
    };
    template<class C, class D> struct TypeIDGetter<std::chrono::time_point<C,D>> {
        static constexpr Arg::TypeID value = Arg::TypeID::TI_EpochNsec;
    };
    template<typename T> struct TypeIDGetter<T*> {
        static constexpr Arg::TypeID value = Arg::TypeID::__ISARRAY | TypeIDGetter<T>::value;
    };
    template<typename T, size_t N> struct TypeIDGetter<T(&)[N]> {
        static constexpr Arg::TypeID value = Arg::TypeID::__ISARRAY | TypeIDGetter<T>::value;
    };
    template<typename T> struct TypeIDGetter<const T> {
        static constexpr Arg::TypeID value = TypeIDGetter<T>::value;
    };

    inline void fillArgsBuffer_sfinae(Arg * arg, std::thread::id && a) {
        arg->type = Arg::TypeID::TI_Thread;
        std::memcpy(&arg->valueOrArray.Thread, &a, sizeof(a));
    }
    inline void fillArgsBuffer_sfinae_pchar(Arg * arg, const char * p) {
        arg->type = static_cast<Arg::TypeID>((int)TypeIDGetter<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->valueOrArray.ArrayPointer = p;
        arg->arrayLength = std::strlen(p);
    }
    inline void fillArgsBuffer_sfinae(Arg * arg, const char * p) {
        fillArgsBuffer_sfinae_pchar(arg, p);
    }
    inline void fillArgsBuffer_sfinae(Arg * arg, char * p) {
        fillArgsBuffer_sfinae_pchar(arg, p);
    }
    template<class C, class D> inline void fillArgsBuffer_sfinae(Arg * arg, std::chrono::time_point<C,D> && a) {
        arg->type = Arg::TypeID::TI_EpochNsec;
        arg->valueOrArray.EpochNsec = a.time_since_epoch().count();
    }
    template<size_t N> inline void fillArgsBuffer_sfinae(Arg * arg, const char (&p)[N]) {
        arg->type = static_cast<Arg::TypeID>((int)TypeIDGetter<char>::value | (int)Arg::TypeID::__ISARRAY);
        arg->valueOrArray.ArrayPointer = &p[0];
        arg->arrayLength = N-1;
    }
    template<class T> inline void fillArgsBuffer_sfinae(Arg * arg, T && a) {
        arg->type = TypeIDGetter<T>::value;
        std::memcpy(&arg->valueOrArray, &a, sizeof(T));
    }

    template<class T, class...Ts> inline void fillArgsBuffer_sfinae(Arg * argBuf, T && fst, Ts &&... args) {
        fillArgsBuffer_sfinae(argBuf++, std::forward<T&&>(fst));
        fillArgsBuffer_sfinae(argBuf, std::forward<Ts&&>(args)...);
    }
    inline void fillArgsBuffer_sfinae(Arg * argBuf) { argBuf->type = Arg::TypeID::TI_NONE; }

} // anonimous namespace

namespace internal {

    template<class...Ts> inline void fillArgsBuffer(Arg * argBuf, Ts &&... args)
    {
        fillArgsBuffer_sfinae(argBuf, std::forward<Ts&&>(args)...);
    }

} // internal namespace

} // logging
} // _utl