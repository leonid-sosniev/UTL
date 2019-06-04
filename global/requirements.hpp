#pragma once

#include <type_traits>
#include <utility>
#include <cstdint>

namespace _utl
{

    namespace detail
    {
        template <class... T>
        struct make_empty
        {
            using type = enum {};
        };
    }

    #define UTL_COMPILES(...) typename _utl::detail::make_empty<decltype(__VA_ARGS__)>::type
    #define UTL_IS_TRUE(...) typename std::enable_if<__VA_ARGS__>::type
    #define UTL_OBJ(...) std::declval<__VA_ARGS__>()
    #define UTL_HAS_TYPE(_1, _2) typename std::enable_if<std::is_same<_1, _2>::value>::type

    template <class Type>
    using RequirePodType =
        UTL_IS_TRUE(
            std::is_pod<Type>::value);

    template <class Type>
    using RequireScalar =
        UTL_IS_TRUE(
            std::is_scalar<Type>::value);

    template <class Type>
    using RequireIntegral =
        UTL_IS_TRUE(
            std::is_integral<Type>::value);

    template <class Type, size_t S>
    using RequireTypeSize =
        UTL_IS_TRUE(
            sizeof(Type) == S);

    template <class Type>
    using RequireSized =
        UTL_COMPILES(
            UTL_OBJ(Type).size());

    template <class Type>
    using RequireClearable =
        UTL_COMPILES(
            UTL_OBJ(Type).clear());

    template <
        class Type,
        class _1 = typename std::decay<decltype(UTL_OBJ(Type).begin())>::type,
        class _2 = typename std::decay<decltype(UTL_OBJ(Type).end())>::type,
        class _3 = typename std::decay<decltype(*UTL_OBJ(_1))>::type,
        class = UTL_IS_TRUE(std::is_same<_1, _2>::value)>
    struct CollectionRequirements
    {
        using type = Type;
        using iterator_type = _1;
        using element_type = _3;
    };

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireInsertable =
        UTL_COMPILES(
            UTL_OBJ(Container).insert(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireBackPushable =
        UTL_COMPILES(
            UTL_OBJ(Container).push_back(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireFrontPushable =
        UTL_COMPILES(
            UTL_OBJ(Container).push_front(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireBackPopable =
        UTL_COMPILES(
            UTL_OBJ(Container).pop_back(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireFrontPopable =
        UTL_COMPILES(
            UTL_OBJ(Container).pop_front(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireBackPopable =
        UTL_COMPILES(
            UTL_OBJ(Container).pop_back(UTL_OBJ(Element)));

    template <
        class Container,
        class Element = typename CollectionRequirements<Container>::element_type>
    using RequireFrontPopable =
        UTL_COMPILES(
            UTL_OBJ(Container).pop_front(UTL_OBJ(Element)));

    template <class Container, class Element>
    using RequireWritable =
        UTL_COMPILES(
            UTL_OBJ(Container).write(UTL_OBJ(const Element *), UTL_OBJ(size_t)));

    template <class Container, class Element>
    using RequireReadable =
        UTL_COMPILES(
            UTL_OBJ(Container).read(UTL_OBJ(Element *), UTL_OBJ(size_t)));

    template <
        class Container, class Element,
        class _1 = decltype( UTL_OBJ(Container).read(UTL_OBJ(Element *), UTL_OBJ(size_t)) ),
        class _2 = typename std::enable_if<std::is_same<size_t,_1>::value>::type
    >
    struct RequireSomeReadable {};

    template <class Container, class Element>
    using RequirePuttable =
        UTL_COMPILES(
            UTL_OBJ(Container).put(UTL_OBJ(Element)));
            
    template <class Container, class Element>
    using RequireGettable =
        typename std::enable_if<
            std::is_convertible<
                decltype(UTL_OBJ(Container).get()), Element>::value,
            Container>::type;

    template <
        class Type,
        class _ = CollectionRequirements<Type>,
        class = RequireSized<Type>>
    struct SizedCollectionRequirements : public _
    {
    };

    template <
        class Type,
        class _ = SizedCollectionRequirements<Type>,
        class = RequireBackPushable<Type>>
    struct BackPushableCollectionRequirements : public _
    {
    };

    template <
        class Type,
        class _ = SizedCollectionRequirements<Type>,
        class = RequireInsertable<Type>>
    struct InsertableCollectionRequirements : public _
    {
    };

    template <
        class Type, class Value,
        class _ = SizedCollectionRequirements<Type>,
        class = typename std::enable_if<std::is_same<typename _::element_type, Value>::value>::type>
    struct TypedCollectionRequirements : public _
    {
    };

    template <
        class Type,
        class _ = SizedCollectionRequirements<Type>,
        class = RequireTypeSize<typename _::element_type, 1>>
    struct ByteCollectionRequirements : public _
    {
    };

    template <
        class Type, class Key, class Value,
        class _1 = SizedCollectionRequirements<Type>,
        class _2 = std::pair<const Key, Value>,
        class _3 = RequireInsertable<Type>,
        class = typename std::enable_if<std::is_same<typename _1::element_type, _2>::value>::type>
    struct KeyValueCollectionRequirements : public _1
    {
    };

    template <
        class Type,
        class _1 = typename std::decay<decltype(*UTL_OBJ(Type))>::type,
        class _2 = typename std::decay<decltype(++UTL_OBJ(Type &))>::type,
        class _3 = typename std::decay<decltype(--UTL_OBJ(Type &))>::type,
        class _4 = typename std::decay<decltype(UTL_OBJ(Type &)++)>::type,
        class _5 = typename std::decay<decltype(UTL_OBJ(Type &)--)>::type,
        class = typename std::enable_if<
            std::is_copy_constructible<Type>::value &&
            std::is_copy_assignable<Type>::value &&
            std::is_destructible<Type>::value>::type,
        class = typename std::enable_if<
            std::is_same<Type, _2>::value &&
            std::is_same<Type, _3>::value &&
            std::is_same<Type, _4>::value &&
            std::is_same<Type, _5>::value>::type>
    struct IteratorRequirements
    {
        using type = Type;
        using value_type = _1;
    };

    #undef UTL_COMPILES
    #undef UTL_IS_TRUE
    #undef UTL_OBJ
    #undef UTL_HAS_TYPE

}
