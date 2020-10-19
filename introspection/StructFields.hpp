#pragma once

#include <cstddef> // size_t
#include <type_traits> // enable_if...

namespace _utl
{
    template<class T> inline constexpr size_t getFieldCount();
    template<class T> inline constexpr size_t getFieldCountRecursive();

    namespace TypeSig
    {
        template<class T> struct Tag {};
        using Id = uint16_t;

        inline constexpr Id type_id(Tag<       void>) { return 'V'; }
        inline constexpr Id type_id(Tag<       char>) { return 'c'; }
        inline constexpr Id type_id(Tag<   char16_t>) { return 'w'; }
        inline constexpr Id type_id(Tag<   char32_t>) { return 'u'; }
        inline constexpr Id type_id(Tag<      float>) { return 'f'; }
        inline constexpr Id type_id(Tag<     double>) { return 'd'; }
        inline constexpr Id type_id(Tag<long double>) { return 'x'; }
        inline constexpr Id type_id(Tag<    uint8_t>) { return 'B'; }
        inline constexpr Id type_id(Tag<     int8_t>) { return 'b'; }
        inline constexpr Id type_id(Tag<   uint16_t>) { return 'H'; }
        inline constexpr Id type_id(Tag<    int16_t>) { return 'h'; }
        inline constexpr Id type_id(Tag<   uint32_t>) { return 'I'; }
        inline constexpr Id type_id(Tag<    int32_t>) { return 'i'; }
        inline constexpr Id type_id(Tag<   uint64_t>) { return 'L'; }
        inline constexpr Id type_id(Tag<    int64_t>) { return 'l'; }
        template<class T> inline constexpr Id type_id(Tag<T>) { return '?'; }

        template<class T> inline constexpr Id type_id(Tag<const T>   ) { return type_id(Tag<T>{}); }
        template<class T> inline constexpr Id type_id(Tag<volatile T>) { return type_id(Tag<T>{}); }
        template<class T> inline constexpr Id type_id(Tag<T&>        ) { return type_id(Tag<T*>{}); }
        template<class T> inline constexpr Id type_id(Tag<T*>        ) { return type_id(Tag<T>{}) + 0x100; }

        template<class Obj, class Ret, class...Args> inline constexpr Id type_id(Tag<Ret(Obj::*)(Args...)>) { return 'M'; }
        template<class Ret, class...Args>            inline constexpr Id type_id(Tag<Ret(*)(Args...)>     ) { return 'F'; }
        template<class T> inline constexpr Id type_id(Tag<typename std::enable_if<std::is_union<T>::value      ,T>::type>) { return 'U'; }
        template<class T> inline constexpr Id type_id(Tag<typename std::enable_if<std::is_enum<T>::value       ,T>::type>) { return 'E'; }
        template<class T> inline constexpr Id type_id(Tag<typename std::enable_if<std::is_polymorphic<T>::value,T>::type>) { return 'P'; }

        template<class T> inline constexpr Id type_id() { return type_id(Tag<T>{}); }

        std::string to_string(Id id)
        {
            std::string result;
            switch (id & 0xFF) {
                case TypeSig::type_id<       void>(): result.append(       "void"); break;
                case TypeSig::type_id<       char>(): result.append(       "char"); break;
                case TypeSig::type_id<   char16_t>(): result.append(   "char16_t"); break;
                case TypeSig::type_id<   char32_t>(): result.append(   "char32_t"); break;
                case TypeSig::type_id<      float>(): result.append(      "float"); break;
                case TypeSig::type_id<     double>(): result.append(     "double"); break;
                case TypeSig::type_id<long double>(): result.append("long double"); break;
                case TypeSig::type_id<    uint8_t>(): result.append(    "uint8_t"); break;
                case TypeSig::type_id<     int8_t>(): result.append(     "int8_t"); break;
                case TypeSig::type_id<   uint16_t>(): result.append(   "uint16_t"); break;
                case TypeSig::type_id<    int16_t>(): result.append(    "int16_t"); break;
                case TypeSig::type_id<   uint32_t>(): result.append(   "uint32_t"); break;
                case TypeSig::type_id<    int32_t>(): result.append(    "int32_t"); break;
                case TypeSig::type_id<   uint64_t>(): result.append(   "uint64_t"); break;
                case TypeSig::type_id<    int64_t>(): result.append(    "int64_t"); break;
                default: result.append("?"); break;
            }
            if (id > 0xFF) {
                size_t N = (id >> 8);
                for (size_t i = 0; i < N; ++i) { result.append("*"); }
            }
            return result;
        }
    };

    namespace details { namespace StructFields
    {
        template<class Pod> struct GetFieldCount {
        private:
            static constexpr const size_t MAX_POSSIBLE_FIELD_COUNT = sizeof(Pod)*8;
            struct Typer {
                const size_t ix;
                template<class T> constexpr operator T () { return T{}; }
            };
            template<class T> struct Result {
                size_t count;
            };
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t I, size_t...Is>
            static constexpr
            auto cnt(std::index_sequence<I,Is...>) -> Result<decltype(Pod{Typer{I},Typer{Is}...})> {
                /* current count is less or equal to target one */
                enum {
                    currCnt = 1+sizeof...(Is),
                    nextCnt = (currCnt + SearchRegion_Max+1) / 2
                };
                if (currCnt == nextCnt) {
                    return Result<Pod>{currCnt};
                } else {
                    return cnt<currCnt,SearchRegion_Max>( std::make_index_sequence<nextCnt>() );
                }
            }
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t...Is>
            static constexpr
            auto cnt(std::index_sequence<Is...>) -> Result<Pod> {
                /* current count is greater than target one */
                enum {
                    currCnt = sizeof...(Is),
                    nextCnt = (SearchRegion_Min + currCnt) / 2
                };
                return cnt<SearchRegion_Min,currCnt-1>(std::make_index_sequence<nextCnt>());
            }
        public:
            static constexpr size_t value = cnt<0,MAX_POSSIBLE_FIELD_COUNT>( std::make_index_sequence<MAX_POSSIBLE_FIELD_COUNT/2>() ).count;
        };

        template<class Pod> struct GetFieldCountRecursive {
        private:
            template<class T, bool isScalar = std::is_scalar<T>::value> struct Selector {};
            template<class T> struct Selector<T,true> {
                static constexpr size_t advancedArg(size_t cnt) { return cnt + 1; }
            };
            template<class T> struct Selector<T,false> {
                static constexpr size_t advancedArg(size_t cnt) { return cnt + getFieldCountRecursive<T>(); }
            };
            struct Typer {
                size_t *cnt, _;
                template<class T> constexpr operator T () { *cnt = Selector<T>::advancedArg(*cnt); return T{}; }
            };
            template<size_t...Is> static constexpr size_t cnt(std::index_sequence<Is...>) {
                size_t N = 0;
                Pod _{ Typer{&N,Is}... };
                return N;
            }
        public:
            static constexpr size_t value = cnt( std::make_index_sequence< getFieldCount<Pod>() >() );
        };
    }} // details // StructFields

    template<class T> inline constexpr size_t getFieldCount() {
        return details::StructFields::GetFieldCount<T>::value;
    }
    template<class T> inline constexpr size_t getFieldCountRecursive() {
        return details::StructFields::GetFieldCountRecursive<T>::value;
    }
} // _utl
