#pragma once

#include <stdexcept>
#include <cstdint>

namespace _utl
{

    void setPreDeallocationCallbacks(
        void(*)(const void * ptr),
        void(*)(const void * ptr, size_t size)
    );
    void setPostDeallocationCallbacks(
        void(*)(const void * ptr),
        void(*)(const void * ptr, size_t size)
    );
    void setPreAllocationCallback(
        void(*)(size_t size)
    );
    void setPostAllocationCallback(
        void(*)(const void * ptr, size_t size)
    );

}
