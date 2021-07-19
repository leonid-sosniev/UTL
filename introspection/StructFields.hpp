#pragma once

#include <cstddef> // size_t
#include <cstdint>
#include <string>
#include <type_traits> // enable_if...

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

        template<class T> inline constexpr size_t getFieldCount() {
            return details::StructFields::GetFieldCount<T>::value;
        }
        template<class T> inline constexpr size_t getFieldCountRecursive() {
            return details::StructFields::GetFieldCountRecursive<T>::value;
        }

        template<class T> struct is_struct {
            static constexpr bool value = std::is_class<T>::value && std::is_trivially_copyable<T>::value;
        };

        struct FieldsMapItem
        {
            TypeSig::Id type;
            uint32_t offset;
        };
        template<class Pod> struct Mapper {
        private:
            template<class T, bool isStruct = is_struct<T>::value> struct Selector {};
            template<class T> struct Selector<T,false>
            {
                static constexpr void detectStructOrOpaque(FieldsMapItem *& item, uint32_t &basicOffset, uint32_t &maxAlignment)
                {
                    // get info on current field
                    item->type = TypeSig::Tag<T>::id;
                    item->offset = (basicOffset + sizeof(T) - 1) / sizeof(T) * sizeof(T); // apply alignment

                    // prepare basic data for the next field
                    basicOffset = item->offset + sizeof(T);
                    maxAlignment = (sizeof(T) > maxAlignment) ? sizeof(T) : maxAlignment;
                    item += 1;
                }
            };
            template<class T> struct Selector<T,true>
            {
                static constexpr void detectStructOrOpaque(FieldsMapItem *& it, uint32_t &basicOffset, uint32_t &maxAlignment)
                {
                    static_assert(std::is_default_constructible<Pod>::value,
                        "PodIntrospection::processStructFields(): given Pod type is not default constructible!");
                    // look ahead and get nested POD alignment
                    uint32_t nestedPodAlignment = 0;
                    uint32_t ignoredOffset = 0;
                    Mapper<T>::getFullMap(it, ignoredOffset, nestedPodAlignment);

                    // take in account this alignment (POD as a whole is aligned as its biggest field)
                    maxAlignment = std::max(nestedPodAlignment, maxAlignment);

                    // go on and save items into the map
                    it = Mapper<T>::getFullMap(it, basicOffset, maxAlignment);
                    basicOffset = (basicOffset + maxAlignment - 1) / maxAlignment * maxAlignment;
                }
            };
            struct Typer {
                FieldsMapItem *& it;
                uint32_t &ofs;
                uint32_t &alg;
                size_t _;
                template<class T> constexpr operator T () { Selector<T>::detectStructOrOpaque(it, ofs, alg); return T{}; }
            };
            template<size_t...Is> static constexpr FieldsMapItem * map(FieldsMapItem *& mapItemsCursor, uint32_t &currentOffset, uint32_t &maxAlignment, std::index_sequence<Is...>)
            {
                Pod _{ Typer{mapItemsCursor,currentOffset,maxAlignment,Is}... };
                return mapItemsCursor;
            }
        public:
            static constexpr FieldsMapItem * getFullMap(FieldsMapItem * map_, uint32_t &offset, uint32_t &maxAlignment) {
                return map(
                    map_, offset, maxAlignment, std::make_index_sequence< getFieldCount<Pod>() >()
                );
            }
        };
        template<class Pod> struct FieldsMapHelper
        {
            static FieldsMapItem MAP[sizeof(Pod) * 8];
            static const FieldsMapItem *m_begin, *m_end;
            static uint32_t _x, _xx;
        };
        template<class Pod> FieldsMapItem FieldsMapHelper<Pod>::MAP[sizeof(Pod) * 8] = {0};
        template<class Pod> uint32_t FieldsMapHelper<Pod>::_x = 0;
        template<class Pod> uint32_t FieldsMapHelper<Pod>::_xx = 0;
        template<class Pod> const FieldsMapItem *FieldsMapHelper<Pod>::m_begin = FieldsMapHelper<Pod>::MAP;
        template<class Pod> const FieldsMapItem *FieldsMapHelper<Pod>::m_end = Mapper<Pod>::getFullMap(FieldsMapHelper<Pod>::MAP, FieldsMapHelper<Pod>::_x, FieldsMapHelper<Pod>::_xx);

        template<class T, class Visitor> inline void processSingleStructField(void * pod, const details::StructFields::FieldsMapItem & it, Visitor & visitor)
        {
            using Byte = enum : uint8_t {};
            static_assert(std::is_unsigned<decltype(it.type)>::value, "Pointer level must be unsigned value. Correct processStructFields().");
            
            auto ptrLvl = it.type >> 8;
            if (!ptrLvl) {
                visitor.process(*reinterpret_cast<T     * const>(reinterpret_cast<Byte *>(pod) + it.offset));
            } else if (ptrLvl ==1) {
                visitor.process(*reinterpret_cast<T    ** const>(reinterpret_cast<Byte *>(pod) + it.offset));
            } else {
                visitor.process(*reinterpret_cast<void ** const>(reinterpret_cast<Byte *>(pod) + it.offset));
            }
        }
        template<class T, class Visitor> inline void processSingleStructField(void * pod, const details::StructFields::FieldsMapItem & it, Visitor & visitor, TypeCategory type)
        {
            using Byte = enum : uint8_t {};
            static_assert(std::is_unsigned<decltype(it.type)>::value, "Pointer level must be unsigned value. Correct processStructFields().");
            
            auto ptrLvl = it.type >> 8;
            if (!ptrLvl) {
                visitor.process(*reinterpret_cast<T     * const>(reinterpret_cast<Byte *>(pod) + it.offset));
            } else if (ptrLvl ==1) {
                visitor.process(*reinterpret_cast<T    ** const>(reinterpret_cast<Byte *>(pod) + it.offset));
            } else {
                visitor.process(*reinterpret_cast<void ** const>(reinterpret_cast<Byte *>(pod) + it.offset));
            }
        }
    }} // details // StructFields

    struct PodIntrospection
    {
        template<class T> static inline constexpr size_t getFieldCount() {
            return details::StructFields::getFieldCount<T>();
        }
        template<class T> static inline constexpr size_t getFieldCountRecursive() {
            return details::StructFields::getFieldCountRecursive<T>();
        }
        template<class Pod> struct StructFieldsMap {
        private:
            static constexpr details::StructFields::FieldsMapHelper<Pod> compileTime = details::StructFields::FieldsMapHelper<Pod>{};
        public:
            using FieldsMapItem = details::StructFields::FieldsMapItem;
            constexpr inline const FieldsMapItem *begin() const { return compileTime.m_begin; }
            constexpr inline const FieldsMapItem *end()   const { return compileTime.m_end;   }
        };

        template<class Visitor, class Pod> static inline void processStructFields(Visitor & visitor, Pod & pod)
        {
            static_assert(std::is_class<Pod>::value,
                "PodIntrospection::processStructFields(): given Pod type is not struct or class!");
            static_assert(std::is_trivially_copyable<Pod>::value,
                "PodIntrospection::processStructFields(): given Pod type is not trivially copyable!");
            static_assert(std::is_default_constructible<Pod>::value,
                "PodIntrospection::processStructFields(): given Pod type is not default constructible!");
            static constexpr StructFieldsMap<Pod> podMap{};
            for (auto &it : podMap)
            {
                switch (it.type & 0xFF)
                {
                    case _utl::TypeSig::type_id<       char>():  details::StructFields::processSingleStructField<       char>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<   char16_t>():  details::StructFields::processSingleStructField<   char16_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<   char32_t>():  details::StructFields::processSingleStructField<   char32_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<      float>():  details::StructFields::processSingleStructField<      float>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<     double>():  details::StructFields::processSingleStructField<     double>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<long double>():  details::StructFields::processSingleStructField<long double>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<    uint8_t>():  details::StructFields::processSingleStructField<    uint8_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<     int8_t>():  details::StructFields::processSingleStructField<     int8_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<   uint16_t>():  details::StructFields::processSingleStructField<   uint16_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<    int16_t>():  details::StructFields::processSingleStructField<    int16_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<   uint32_t>():  details::StructFields::processSingleStructField<   uint32_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<    int32_t>():  details::StructFields::processSingleStructField<    int32_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<   uint64_t>():  details::StructFields::processSingleStructField<   uint64_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::type_id<    int64_t>():  details::StructFields::processSingleStructField<    int64_t>(&pod, it, visitor); break;
                    case _utl::TypeSig::method_type_id():    break;
                    case _utl::TypeSig::function_type_id():  break;
                    case _utl::TypeSig::union_type_id():     break;
                    case _utl::TypeSig::enum_type_id():      break;
                    case _utl::TypeSig::polymorph_type_id(): break;
                    case _utl::TypeSig::class_type_id():     break;
                    default: std::abort(); break;
                }
            }
        }
    };

} // _utl
