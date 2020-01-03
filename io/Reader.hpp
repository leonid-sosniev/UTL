#pragma once
#include <stdint.h>

namespace _utl
{

    class AbstractReader {
    public:
        virtual uint32_t read(void * data, uint32_t maxSize) = 0;
        virtual ~AbstractReader() {}
    };

}