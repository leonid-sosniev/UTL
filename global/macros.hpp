#pragma once

#include <cstring>

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#   define UTL_CURR_FUNC __PRETTY_FUNCTION__

#elif defined(__DMC__) && (__DMC__ >= 0x810)
#   define UTL_CURR_FUNC __PRETTY_FUNCTION__

#elif defined(__FUNCSIG__)
#   define UTL_CURR_FUNC __FUNCSIG__

#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#   define UTL_CURR_FUNC __FUNCTION__

#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#   define UTL_CURR_FUNC __FUNC__

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#   define UTL_CURR_FUNC __func__

#elif defined(__cplusplus) && (__cplusplus >= 201103)
#   define UTL_CURR_FUNC __func__

#else
#   define UTL_CURR_FUNC "(unknown)"

#endif

namespace _utl {

    //class CurrentFunction
    //{
    //    char * const stdFunction;
    //
    //    template<size_t > constexpr CurrentFunction(const char * __function__)
    //        :
    //    {
    //        stdFunction(__function__)
    //    }
    //    constexpr const char * name() { return ""; }
    //    constexpr const char * scope() { return ""; }
    //    constexpr const char * fullName() { return ""; }
    //    constexpr const char * args()
    //    {
    //        const char * parOp = std::strchr(stdFunction, '(');
    //        const char * parCl = std::strchr(parOp, ')');
    //        const auto len =
    //    }
    //    constexpr const char * returnType() { return ""; }
    //    constexpr const char * keywords() { return ""; }
    //};
    //void f() {
    //    constexpr static CurrentFunction CF{UTL_CURR_FUNC};
    //    CF.stdFunction;
    //}

}
