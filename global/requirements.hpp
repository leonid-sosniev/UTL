#pragma once

#include <type_traits>
#include <utility>
#include <cstdint>

namespace _utl {

    template<class object_t> using ScalarType = typename std::enable_if<
        std::is_scalar<object_t>::value,
        object_t
    >::type;

    template<class object_t> using IntegralType = typename std::enable_if<
        std::is_integral<object_t>::value,
        object_t
    >::type;

    template<class object_t, size_t S> using OfSize = typename std::enable_if<
        sizeof(object_t) == S,
        object_t
    >::type;

    template<class object_t> using EnumerableType = typename std::enable_if<
        std::is_same<
            typename std::decay<decltype(std::declval<object_t>().begin())>::type,
            typename std::decay<decltype(std::declval<object_t>().end())>::type
        >::value,
        object_t
    >::type;

    template<class object_t, class value_t> using EnumerableSequenceType = typename std::enable_if<
        std::is_same<
            typename std::decay<decltype(*std::declval<object_t>().begin())>::type,
            typename std::decay<decltype(*std::declval<object_t>().end())>::type
        >::value && std::is_same<
            typename std::decay<decltype(*std::declval<object_t>().begin())>::type,
            typename std::decay<value_t>::type
        >::value,
        object_t
    >::type;

    template<class object_t, class key_t, class value_t> using EnumerableMappingType = typename std::enable_if<
        std::is_same<
            typename std::decay<decltype(*std::declval<object_t>().begin())>::type,
            typename std::decay<decltype(*std::declval<object_t>().end())>::type
        >::value && std::is_same<
            typename std::decay<decltype(*std::declval<object_t>().begin())>::type,
            typename std::decay<std::pair<const key_t,value_t>>::type
        >::value,
        object_t
    >::type;

    template<class object_t> using PodType = typename std::enable_if<  std::is_pod<object_t>::value  >::type;

}
