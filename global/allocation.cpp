#include <cstdlib>
#include <chrono>
#include <new>
#include "allocation.hpp"

using namespace std;

void deallocate_nothrow(void * ptr)
{
    std::free(ptr);
}
void deallocate_nothrow(void * ptr, size_t size)
{
    std::free(ptr);
}
void * allocate_nothrow(size_t size)
{
    void * ptr = std::malloc(size);
    return ptr;
}
void * allocate(size_t size)
{
    void * ptr = allocate_nothrow(size);
    if (ptr) {
        return ptr;
    } else {
        throw std::bad_alloc();
    }
}


void * operator new(size_t size)
{
    return allocate(size);
}
void * operator new(size_t size, std::nothrow_t const&) noexcept
{
    return allocate_nothrow(size);
}

void * operator new[](size_t size)
{
    return allocate(size);
}
void * operator new[](size_t size, std::nothrow_t const&) noexcept
{
    return allocate_nothrow(size);
}

void operator delete(void * ptr) noexcept
{
    deallocate_nothrow(ptr);
}
void operator delete(void * ptr, size_t size) noexcept
{
    deallocate_nothrow(ptr, size);
}
void operator delete(void * ptr, std::nothrow_t const&) noexcept
{
    deallocate_nothrow(ptr);
}

void operator delete[](void * ptr) noexcept
{
    deallocate_nothrow(ptr);
}
void operator delete[](void * ptr, size_t size) noexcept
{
    deallocate_nothrow(ptr, size);
}
void operator delete[](void * ptr, std::nothrow_t const&) noexcept
{
    deallocate_nothrow(ptr);
}

