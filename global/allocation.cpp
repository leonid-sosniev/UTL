#include <cstdlib>
#include <chrono>
#include <new>
#include "allocation.hpp"

using namespace std;

void empty_s(size_t size) {}
void empty_p(const void * ptr) {}
void empty_ps(const void * ptr, size_t size) {}

static void (*pre_dealloc_callback)(const void * ptr) = empty_p;
static void (*pre_dealloc_size_callback)(const void * ptr, size_t size) = empty_ps;
static void (*pre_alloc_callback)(size_t size) = empty_s;

static void (*post_dealloc_callback)(const void * ptr) = empty_p;
static void (*post_dealloc_size_callback)(const void * ptr, size_t size) = empty_ps;
static void (*post_alloc_callback)(const void * ptr, size_t size) = empty_ps;


void _utl::setPreDeallocationCallbacks(
    void(*pointerOnly)(const void * ptr),
    void(*pointerAndSize)(const void * ptr, size_t size)
){
    if (!pointerAndSize) { std::abort(); }
    if (!pointerOnly) { std::abort(); }
    pre_dealloc_callback = pointerOnly;
    pre_dealloc_size_callback = pointerAndSize;
}
void _utl::setPostDeallocationCallbacks(
    void(*pointerOnly)(const void * ptr),
    void(*pointerAndSize)(const void * ptr, size_t size)
){
    if (!pointerAndSize) { std::abort(); }
    if (!pointerOnly) { std::abort(); }
    post_dealloc_callback = pointerOnly;
    post_dealloc_size_callback = pointerAndSize;
}
void _utl::setPreAllocationCallback(void(*sizeOnly)(size_t size)) {
    if (!sizeOnly) { std::abort(); }
    pre_alloc_callback = sizeOnly;
}
void _utl::setPostAllocationCallback(void(*pointerAndSize)(const void * ptr, size_t size)) {
    if (!pointerAndSize) { std::abort(); }
    post_alloc_callback = pointerAndSize;
}



void deallocate_nothrow(void * ptr) {
    pre_dealloc_callback(ptr);
    std::free(ptr);
    post_dealloc_callback(ptr);
}
void deallocate_nothrow(void * ptr, size_t size) {
    pre_dealloc_size_callback(ptr,size);
    std::free(ptr);
    post_dealloc_size_callback(ptr,size);
}
void * allocate_nothrow(size_t size) {
    pre_alloc_callback(size);
    void * ptr = std::malloc(size);
    post_alloc_callback(ptr,size);
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

