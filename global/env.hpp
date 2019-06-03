#pragma once

#include <limits.h>
#include <stdint.h>

namespace _utl
{

    enum class HostOrder : uint32_t
    {
        LITTLE_ENDIAN      = 0x03020100ul,
        BIG_ENDIAN         = 0x00010203ul,
        WORD_BIG_ENDIAN    = 0x02030001ul, /* Honeywell 316 */
        WORD_LITTLE_ENDIAN = 0x01000302ul  /* DEC PDP-11 */
    };
    static const union {
        uint8_t bytes[4];
        HostOrder value;
    }
    hostByteEndianness = { { 0, 1, 2, 3 } };


    enum class OS
    {
        Linux       ,   //  __linux__
        Android     ,   //  __ANDROID__ (implies __linux__)
        Darwin      ,   //  __APPLE__
        Akaros      ,   //  __ros__
        Windows32   ,   //  _WIN32
        Windows64   ,   //  _WIN64 (implies _WIN32)
        NaCL        ,   //  __native_client__
        AsmJS       ,   //  __asmjs__
        Fuschia     ,   //  __Fuchsia__
        Undefined
    }
    operatingSystem =
    #if defined(__linux__)
        #if defined(__ANDROID__)
            OS::Android
        #else
            OS::Linux
        #endif
    #elif defined(__APPLE__)
        OS::Darwin
    #elif defined(__ros__)
        OS::Akaros
    #elif defined(_WIN32)
        #if defined(_WIN64)
            OS::Windows64
        #else
            OS::Windows32
        #endif
    #elif defined(__native_client__)
        OS::NaCL
    #elif defined(__asmjs__)
        OS::AsmJS
    #elif defined(__Fuchsia__)
        OS::Fuschia
    #else
        OS::Undefined
    #endif
    ;


    enum class CppStandard
    {
        CppPre98 = 1,
        Cpp98    = 199711L,
        Cpp11    = 201103L,
        Cpp14    = 201402L,
        Cpp17    = 201703L
    }
    cppStandard = (CppStandard) __cplusplus;

}