#pragma once

#include <thread>
#include <utl/io/SharedMemoryRegion.hpp>
#include <utl/io/CircularBuffer.hpp>
#include <utl/threading/Spinlock.hpp>
#if defined(_WIN32)
#   include <windows.h>
    using MUX_ID = HANDLE;
#elif defined(__linux__)
#   include <semaphore.h>
    using MUX_ID = sem_t*;
#endif

namespace _utl
{

    class CircularBufferOnSharedMemory : public _utl::AbstractReader, public _utl::AbstractWriter {
    private:
        struct LockingBuffer {
            _utl::SpinLock spin;
            _utl::CircularBuffer buf;
        };
        CircularBufferOnSharedMemory(_utl::SharedMemoryRegion && mem, LockingBuffer * spinbuf, bool ownsMem)
            : spinbuf(spinbuf)
            , mem(std::move(mem))
            , thisBufferCreatedTheMemoryRegion(ownsMem)
        {}
        static void lockMux(MUX_ID mux) {
#if defined(_WIN32)
            WaitForSingleObject(mux, INFINITE);
#elif defined(__linux__)
            int i, s;
            s = sem_getvalue(mux, &i);
            s = sem_wait(mux);
#endif
        }
        static void unlockMux(MUX_ID mux) {
#if defined(_WIN32)
            ReleaseMutex(mux);
#elif defined(__linux__)
            int i, s;
            s = sem_getvalue(mux, &i);
            s = sem_post(mux);
#endif
        }
        MUX_ID mux;
        _utl::SharedMemoryRegion mem;
        LockingBuffer * spinbuf;
        bool thisBufferCreatedTheMemoryRegion;
    public:
        virtual ~CircularBufferOnSharedMemory()
        {
            if (thisBufferCreatedTheMemoryRegion) {
                spinbuf->buf.CircularBuffer::~CircularBuffer();
            }
#if defined(_WIN32)
            CloseHandle(mux);
#elif defined(__linux__)
            sem_close(mux);
#endif
        }
        CircularBufferOnSharedMemory(CircularBufferOnSharedMemory && rhs)
            : spinbuf(rhs.spinbuf)
            , mux(rhs.mux)
            , mem(std::move(rhs.mem))
        {
            memset(&rhs, 0, sizeof(rhs));
        }
        static CircularBufferOnSharedMemory create(const std::string & name, uint32_t maxDataSize)
        {
            std::string muxName = "/" + name;
#if defined(_WIN32)
            MUX_ID mux = CreateMutexA(NULL, false, muxName.data()); // create unlocked
#elif defined(__linux__)
            auto mux = sem_open(muxName.data(), O_CREAT, 0666, 1); // create unlocked
#endif
            lockMux(mux);

            auto circularBufMemSize = _utl::CircularBuffer::getMemorySizeRequired(maxDataSize);
            auto totalMemSize = sizeof(_utl::SpinLock) + circularBufMemSize;

            auto mem = _utl::SharedMemoryRegion::create(name, totalMemSize, _utl::SharedMemoryRegion::AccessMode::ReadWrite);
            auto lkBufRgn = static_cast<LockingBuffer*>(mem.data());
            new (&lkBufRgn->spin) _utl::SpinLock();
            auto cbuf = _utl::CircularBuffer::createInPlace(circularBufMemSize, &lkBufRgn->buf, [](void*){});

            unlockMux(mux);

            return CircularBufferOnSharedMemory(std::move(mem), lkBufRgn, true);
        }
        static CircularBufferOnSharedMemory openExisting(const std::string & memName, uint32_t memSize)
        {
#if defined(_WIN32)
            auto mux = OpenMutexA(MUTEX_ALL_ACCESS, false, memName.data());
#elif defined(__linux__)
            std::string muxName = "/" + memName;
            auto mux = sem_open(muxName.data(), 0);
#endif
            lockMux(mux);
            auto mem = _utl::SharedMemoryRegion::openExisting(memName, memSize, _utl::SharedMemoryRegion::AccessMode::ReadWrite);
            auto spinbuf = static_cast<LockingBuffer*>(mem.data());
            unlockMux(mux);
            return CircularBufferOnSharedMemory(std::move(mem), spinbuf, false);
        }
        uint32_t write(const void * data, uint32_t size) override {
            spinbuf->spin.lock();
            auto l = spinbuf->buf._utl::CircularBuffer::write(data, size);
            spinbuf->spin.unlock();
            return l;
        }
        uint32_t read(void * data, uint32_t maxSize) override {
            spinbuf->spin.lock();
            auto l = spinbuf->buf._utl::CircularBuffer::read(data, maxSize);
            spinbuf->spin.unlock();
            return l;
        }
        bool flush() override { return true; }
        uint32_t capacity() const { return spinbuf->buf.capacity(); }
        uint32_t size() const { return spinbuf->buf.size(); }
    };

}