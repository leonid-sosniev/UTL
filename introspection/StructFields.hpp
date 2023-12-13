#pragma once

#include <cstddef> // size_t
#include <cstdint>
#include <string>
#include <type_traits> // enable_if...
#include <utility>

#undef max

namespace _utl
{
    namespace TypeSig
    {
        using Id = uint16_t;
        inline constexpr Id method_type_id()    { return 'M'; }
        inline constexpr Id function_type_id()  { return 'F'; }
        inline constexpr Id union_type_id()     { return 'U'; }
        inline constexpr Id enum_type_id()      { return 'E'; }
        inline constexpr Id class_type_id()     { return 'C'; }
        inline constexpr Id polymorph_type_id() { return 'P'; }
        template<class T> class Tag {
        private:
            template<class U> static constexpr typename std::enable_if<std::is_union<U>::value, Id>::type
            getId(const U &&) {
                return TypeSig::union_type_id();
            }
            template<class U> static constexpr typename std::enable_if<std::is_enum<U>::value, Id>::type
            getId(const U &&) {
                return TypeSig::enum_type_id();
            }
            template<class U> static constexpr typename std::enable_if<std::is_class<U>::value && !std::is_enum<T>::value, Id>::type
            getId(const U &&) {
                return TypeSig::class_type_id();
            }
            template<class U> static constexpr typename std::enable_if<std::is_polymorphic<U>::value, Id>::type
            getId(const U &&) {
                return TypeSig::polymorph_type_id();
            }
        public:
            static constexpr Id id = getId(T{});
        };
        template<> struct Tag<       void> { static constexpr Id id = 'V'; };
        template<> struct Tag<       char> { static constexpr Id id = 'c'; };
        template<> struct Tag<   char16_t> { static constexpr Id id = 'w'; };
        template<> struct Tag<   char32_t> { static constexpr Id id = 'u'; };
        template<> struct Tag<      float> { static constexpr Id id = 'f'; };
        template<> struct Tag<     double> { static constexpr Id id = 'd'; };
        template<> struct Tag<long double> { static constexpr Id id = 'x'; };
        template<> struct Tag<    uint8_t> { static constexpr Id id = 'B'; };
        template<> struct Tag<     int8_t> { static constexpr Id id = 'b'; };
        template<> struct Tag<   uint16_t> { static constexpr Id id = 'H'; };
        template<> struct Tag<    int16_t> { static constexpr Id id = 'h'; };
        template<> struct Tag<   uint32_t> { static constexpr Id id = 'I'; };
        template<> struct Tag<    int32_t> { static constexpr Id id = 'i'; };
        template<> struct Tag<   uint64_t> { static constexpr Id id = 'L'; };
        template<> struct Tag<    int64_t> { static constexpr Id id = 'l'; };
        template<class T> struct Tag<const T>    { static constexpr Id id = Tag<T>::id; };
        template<class T> struct Tag<volatile T> { static constexpr Id id = Tag<T>::id; };
        template<class T> struct Tag<T&>         { static constexpr Id id = Tag<T*>::id; };
        template<class T> struct Tag<T*>         { static constexpr Id id = Tag<T>::id + 0x100; };

        template<class Obj, class Ret, class...Args> struct Tag<Ret(Obj::*)(Args...)> {
            static constexpr Id id = method_type_id();
        };
        template<class Ret, class...Args> struct Tag<Ret(*)(Args...)> {
            static constexpr Id id = function_type_id();
        };

        template<class T> inline constexpr Id type_id() { return Tag<T>::id; }

        inline std::string to_string(Id id)
        {
            std::string result;
            switch (id & 0xFF) {
                case TypeSig::Tag<       void>::id: result.append(       "void"); break;
                case TypeSig::Tag<       char>::id: result.append(       "char"); break;
                case TypeSig::Tag<   char16_t>::id: result.append(   "char16_t"); break;
                case TypeSig::Tag<   char32_t>::id: result.append(   "char32_t"); break;
                case TypeSig::Tag<      float>::id: result.append(      "float"); break;
                case TypeSig::Tag<     double>::id: result.append(     "double"); break;
                case TypeSig::Tag<long double>::id: result.append("long double"); break;
                case TypeSig::Tag<    uint8_t>::id: result.append(    "uint8_t"); break;
                case TypeSig::Tag<     int8_t>::id: result.append(     "int8_t"); break;
                case TypeSig::Tag<   uint16_t>::id: result.append(   "uint16_t"); break;
                case TypeSig::Tag<    int16_t>::id: result.append(    "int16_t"); break;
                case TypeSig::Tag<   uint32_t>::id: result.append(   "uint32_t"); break;
                case TypeSig::Tag<    int32_t>::id: result.append(    "int32_t"); break;
                case TypeSig::Tag<   uint64_t>::id: result.append(   "uint64_t"); break;
                case TypeSig::Tag<    int64_t>::id: result.append(    "int64_t"); break;
                case TypeSig::method_type_id   ():    result.append(     "method"); break;
                case TypeSig::function_type_id ():    result.append(   "function"); break;
                case TypeSig::union_type_id    ():    result.append(      "union"); break;
                case TypeSig::enum_type_id     ():    result.append(       "enum"); break;
                case TypeSig::class_type_id    ():    result.append(      "class"); break;
                case TypeSig::polymorph_type_id():    result.append(  "polymorph"); break;
                default: result.append("?"); break;
            }
            if (id > 0xFF) {
                size_t N = (id >> 8);
                for (size_t i = 0; i < N; ++i) { result.append("*"); }
            }
            return result;
        }

    }

    namespace details { namespace StructFields
    {
        template<class T> inline constexpr size_t getFieldCount();
        template<class T> inline constexpr size_t getFieldCountRecursive();

        template<class Pod> struct GetFieldCount {
        private:
            static constexpr const size_t MAX_POSSIBLE_FIELD_COUNT = sizeof(Pod)*8;
            struct Typer {
                const size_t ix;
                template<class T> constexpr operator T () { return T{}; }
            };
            template<class T> struct Result {
                const size_t count;
                constexpr Result(const size_t count_) : count(count_) {}
            };
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t I, size_t...Is>
            static constexpr auto probeWithBinarySearch(std::index_sequence<I,Is...>) -> Result<const decltype(Pod{Typer{I},Typer{Is}...})>
            {
                constexpr size_t currCnt = 1+sizeof...(Is);
                constexpr size_t nextCnt = (currCnt + SearchRegion_Max+1) / 2;
                return (currCnt == nextCnt)
                    ? Result<const Pod>{currCnt}
                    : probeWithBinarySearch<currCnt,SearchRegion_Max>(std::make_index_sequence<nextCnt>());
            }
            template<size_t SearchRegion_Min, size_t SearchRegion_Max, size_t...Is>
            static constexpr auto probeWithBinarySearch(std::index_sequence<Is...>) -> Result<const Pod>
            {
                constexpr size_t currCnt = sizeof...(Is);
                constexpr size_t nextCnt = (SearchRegion_Min + currCnt) / 2;
                return probeWithBinarySearch<SearchRegion_Min,currCnt-1>(std::make_index_sequence<nextCnt>());
            }
        public:
            static constexpr size_t value = probeWithBinarySearch<0,MAX_POSSIBLE_FIELD_COUNT>( std::make_index_sequence<MAX_POSSIBLE_FIELD_COUNT/2>() ).count;
        };

        template<class Pod> struct GetFieldCountRecursive {
        private:
            template<class T, bool isScalar = std::is_scalar<T>::value || std::is_union<T>::value> struct Selector {};
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

        template<class T> inline constexpr size_t getFieldCount() {
            return details::StructFields::GetFieldCount<T>::value;
        }
        template<class T> inline constexpr size_t getFieldCountRecursive() {
            return details::StructFields::GetFieldCountRecursive<T>::value;
        }

        template<class T> struct is_struct {
            static constexpr bool value = std::is_class<T>::value && std::is_trivially_copyable<T>::value && std::is_default_constructible<T>::value;
        };

        template<class T> inline T * alignedPtr(void * ptr, size_t align)
        {
            auto p = reinterpret_cast<uintptr_t>(ptr);
            p = (p + align - 1) / align * align;
            return reinterpret_cast<T*>(p);
        }

        template<class Pod, class Visitor> struct FieldsProcessor {
        private:
            template<class T, bool isStruct = is_struct<T>::value> struct Selector
            {};
            template<class T> struct Selector<T,false> {
                static void * selectThenAlignThenProcess(void * cursor, Visitor & visitor) {
                    T * field = alignedPtr<T>(cursor, alignof(T));
                    visitor.process(*field);
                    return field + 1;
                }
            };
            template<class T> struct Selector<T,true> {
                static void * selectThenAlignThenProcess(void * cursor, Visitor & visitor) {
                    T * field = alignedPtr<T>(cursor, alignof(T));
                    visitor.process(*field);
                    return alignedPtr<uint8_t>(++field, alignof(T));
                }
            };
            struct Typer {
                void ** cursorPtr;
                Visitor & visitor;
                size_t _;
                template<class T> operator T () {
                    *cursorPtr = Selector<T>::selectThenAlignThenProcess(*cursorPtr, visitor); return T{};
                }
            };
            template<class P> struct Iterator {
                template<size_t...Is> static void * iter(void * cursor, Visitor & visitor, std::index_sequence<Is...>) {
                    P _{ Typer{&cursor,visitor,Is}... };
                    return cursor;
                }
                static void * iterate(void * cursor, Visitor & visitor) {
                    return iter(cursor, visitor, std::make_index_sequence< getFieldCount<P>() >());
                }
            };
        public:
            static void processTopLevelFields(Pod & pod, Visitor & visitor) {
                void * cursor = const_cast<void*>(static_cast<const void *>(&pod));
                Iterator<Pod>::iterate(cursor, visitor);
            }
        };

    }} // details // StructFields

    struct PodIntrospection
    {
        template<class T> static inline constexpr size_t getFieldCount() {
            return details::StructFields::getFieldCount<const T>();
        }
        template<class T> static inline constexpr size_t getFieldCountRecursive() {
            return details::StructFields::getFieldCountRecursive<const T>();
        }
        template<class Pod> struct StructFieldsMap {
            using FieldsMapItem = int;
            constexpr inline const FieldsMapItem *begin() const { return nullptr; }
            constexpr inline const FieldsMapItem *end()   const { return nullptr; }
        };

        /**
        This iterates over the fields of the given POD object and calls Visitor::process(T&) to perform some actions for every field
        In order to iterate also over nested structures fields the Visitor class must include process<is_class>(T&) overload
        */
        template<class Visitor, class Pod> static inline void processTopLevelFields(Visitor & visitor, Pod & pod)
        {
            static_assert(std::is_trivially_copyable<Pod>::value,
                "PodIntrospection::processTopLevelFields(): given Pod type is not trivially copyable!");
            static_assert(std::is_default_constructible<Pod>::value,
                "PodIntrospection::processTopLevelFields(): given Pod type is not default constructible!");
            
            _utl::details::StructFields::FieldsProcessor<Pod,Visitor>::processTopLevelFields(pod, visitor);
        }
    };

} // _utl
