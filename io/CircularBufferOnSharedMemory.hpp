#pragma once

#include <thread>
#include <mutex>
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


#if defined(UTL_LOG_SHARED_MEMORY_REGION)
#  define UTL_SHARED_MEMORY_REGION_LOG(msg, ...) UTL_logev(Spam, "[$f - $l:$m; $t] " msg ,##__VA_ARGS__)
#else
#  define UTL_SHARED_MEMORY_REGION_LOG(msg, ...)
#endif

namespace _utl
{

    class NamedMutex {
    private:
#if defined(_WIN32)
        using MUX_ID = HANDLE;
#elif defined(__linux__)
        using MUX_ID = sem_t*;
#endif
        MUX_ID mux;
        explicit NamedMutex(MUX_ID mux) : mux(mux)
        {
            if (mux == (MUX_ID) 0)
            {
                UTL_SHARED_MEMORY_REGION_LOG("Refusing construction of NULL mutex (throw) errno=$WI", errno);
                throw std::system_error{ std::error_code{errno,std::system_category()} };
            }
            UTL_SHARED_MEMORY_REGION_LOG("Mutex (h=$LU) constructed.", (uint64_t) mux);
        }
    public:
        static void removeName(const std::string & name)
        {
            UTL_SHARED_MEMORY_REGION_LOG("sem_unlink($ZS)...", name.data());
            int s = sem_unlink(name.data());
        }
        static NamedMutex createNew(const std::string & name_)
        {
            UTL_SHARED_MEMORY_REGION_LOG("Creating new mutex '$ZS'...", name_.c_str());
            #if defined(_WIN32)
            return NamedMutex{
                CreateMutexA(NULL, false, name.c_str())
            };
            #elif defined(__linux__)
            auto name = (name_.size() && name_[0] != '/') ? "/" + name_ : name_;
            return NamedMutex{
                sem_open(name.c_str(), O_CREAT | O_EXCL, 0666, 1)
            };
            #endif
        }
        static NamedMutex openExisting(const std::string & name_)
        {
            UTL_SHARED_MEMORY_REGION_LOG("Opening existing mutex '$ZS'...", name_.c_str());
            #if defined(_WIN32)
            return NamedMutex{
                OpenMutexA(MUTEX_ALL_ACCESS, false, name.c_str())
            };
            #elif defined(__linux__)
            auto name = (name_.size() && name_[0] != '/') ? "/" + name_ : name_;
            return NamedMutex{
                sem_open(name.c_str(), 0)
            };
            #endif
        }
        
        NamedMutex() : mux((MUX_ID) 0)
        {
            UTL_SHARED_MEMORY_REGION_LOG("NULL mutex constructed");
        }
        NamedMutex(NamedMutex && rhs) : mux(rhs.mux)
        {
            UTL_SHARED_MEMORY_REGION_LOG("moving mutex (h=$LU)...", (uint64_t) rhs.mux);
            rhs.mux = (MUX_ID) 0;
        }
        NamedMutex & operator = (NamedMutex && rhs)
        {
            UTL_SHARED_MEMORY_REGION_LOG("moving mutex (h=$LU)...", (uint64_t) rhs.mux);
            mux = rhs.mux;
            rhs.mux = (MUX_ID) 0;
            return *this;
        }
        NamedMutex & operator = (const NamedMutex & rhs) = delete;
        ~NamedMutex()
        {
            if (mux) {
                UTL_SHARED_MEMORY_REGION_LOG("closing mutex (h=$LU)", (uint64_t) mux);
                #if defined(_WIN32)
                CloseHandle(mux);
                #elif defined(__linux__)
                auto s = sem_close(mux);
                if (s < 0) {
                    UTL_SHARED_MEMORY_REGION_LOG("mutex closing threw: $WI", errno);
                    throw std::system_error{ std::error_code{errno,std::system_category()} };
                }
                #endif
            }
            else { UTL_SHARED_MEMORY_REGION_LOG("closing null mutex)"); }
        }
        
        void lock()
        {
            UTL_SHARED_MEMORY_REGION_LOG("locking mutex (h=$LU)...", (uint64_t) mux);
            #if defined(_WIN32)
            WaitForSingleObject(mux, INFINITE);
            #elif defined(__linux__)
            int s = sem_wait(mux);
            if (s < 0) {
                UTL_SHARED_MEMORY_REGION_LOG("mutex locking ($LU) threw: $WI", (uint64_t) mux, errno);
                throw std::system_error{ std::error_code{errno,std::system_category()} };
            }
            #endif
            UTL_SHARED_MEMORY_REGION_LOG("mutex locked (h=$LU).", (uint64_t) mux);
        }
        bool try_lock()
        {
            UTL_SHARED_MEMORY_REGION_LOG("trying to lock mutex (h=$LU)...", (uint64_t) mux);
            #if defined(_WIN32)
            auto s = WaitForSingleObject(mux, 0);
            if (s == WAIT_OBJECT_0) {
                UTL_SHARED_MEMORY_REGION_LOG("locking successful (h=$LU)...", (uint64_t) mux);
                return true;
            }
            if (s == WAIT_TIMEOUT) {
                UTL_SHARED_MEMORY_REGION_LOG("locking failed (h=$LU)...", (uint64_t) mux);
                return false;
            }
            if (s == WAIT_ABANDONED) {
                UTL_SHARED_MEMORY_REGION_LOG("locking threw (h=$LU)...", (uint64_t) mux);
                throw std::system_error{ std::error_code{WAIT_ABANDONED,std::system_category()} };
            } else {
                UTL_SHARED_MEMORY_REGION_LOG("locking threw (h=$LU)...", (uint64_t) mux);
                throw std::system_error{ std::error_code{GetLastError(),std::system_category()} };
            }
            #elif defined(__linux__)
            int s = sem_trywait(mux);
            if (s < 0) {
                if (errno == EAGAIN) {
                    UTL_SHARED_MEMORY_REGION_LOG("locking failed (h=$LU)...", (uint64_t) mux);
                    return false;
                }
                UTL_SHARED_MEMORY_REGION_LOG("locking threw (h=$LU)...", (uint64_t) mux);
                throw std::system_error{ std::error_code{errno,std::system_category()} };
            }
            UTL_SHARED_MEMORY_REGION_LOG("locking successful (h=$LU)...", (uint64_t) mux);
            return true;
            #endif
        }
        void unlock()
        {
            UTL_SHARED_MEMORY_REGION_LOG("unlocking mutex (h=$LU)...", (uint64_t) mux);
            #if defined(_WIN32)
            ReleaseMutex(mux);
            #elif defined(__linux__)
            int s = sem_post(mux);
            if (s < 0) {
                UTL_SHARED_MEMORY_REGION_LOG("mutex UNlocking ($LU) threw: $WI", (uint64_t) mux, errno);
                throw std::system_error{ std::error_code{errno,std::system_category()} };
            }
            #endif
            UTL_SHARED_MEMORY_REGION_LOG("mutex unlocked (h=$LU)...", (uint64_t) mux);
        }
    };

    class CircularBufferOnSharedMemory : public _utl::AbstractReader, public _utl::AbstractWriter {
    private:
        struct LockingBuffer {
            _utl::SpinLock spin;
            _utl::CircularBuffer buf;
        };
        CircularBufferOnSharedMemory(_utl::SharedMemoryRegion && mem, LockingBuffer * spinbuf, std::string ownedMutexName = "")
            : spinbuf(spinbuf)
            , mem(std::move(mem))
            , ownedMutexName(std::move(ownedMutexName))
        { UTL_SHARED_MEMORY_REGION_LOG(""); }
        NamedMutex mux;
        _utl::SharedMemoryRegion mem;
        LockingBuffer * spinbuf;
        std::string ownedMutexName;
    public:
        virtual ~CircularBufferOnSharedMemory()
        {
            if (ownedMutexName.size()) {
                UTL_SHARED_MEMORY_REGION_LOG("CircularBuffer destruction");
                spinbuf->buf.CircularBuffer::~CircularBuffer();
                #if defined(__linux__)
                UTL_SHARED_MEMORY_REGION_LOG("sem_unlink($ZS)...", ownedMutexName.data());
                int s = sem_unlink(ownedMutexName.data());
                #endif
            }
            UTL_SHARED_MEMORY_REGION_LOG("");
        }
        CircularBufferOnSharedMemory(CircularBufferOnSharedMemory && rhs)
            : spinbuf(rhs.spinbuf)
            , mux(std::move(rhs.mux))
            , mem(std::move(rhs.mem))
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
            memset(&rhs, 0, sizeof(rhs));
        }
        static CircularBufferOnSharedMemory create(const std::string & name, uint32_t maxDataSize)
        {
            auto mux = NamedMutex::createNew(name);
            std::unique_lock<NamedMutex> lock(mux);

            auto circularBufMemSize = _utl::CircularBuffer::getMemorySizeRequired(maxDataSize);
            auto totalMemSize = sizeof(_utl::SpinLock) + circularBufMemSize;
            UTL_SHARED_MEMORY_REGION_LOG("buf sz: $WU, mem sz: $WU. Creating buf in mem...", circularBufMemSize, totalMemSize);

            auto mem = _utl::SharedMemoryRegion::create(name, totalMemSize, _utl::SharedMemoryRegion::AccessMode::ReadWrite);
            auto lkBufRgn = static_cast<LockingBuffer*>(mem.data());
            new (&lkBufRgn->spin) _utl::SpinLock();
            auto cbuf = _utl::CircularBuffer::createInPlace(circularBufMemSize, &lkBufRgn->buf, [](void*){});

            return CircularBufferOnSharedMemory(std::move(mem), lkBufRgn, name);
        }
        static CircularBufferOnSharedMemory openExisting(const std::string & name, uint32_t memSize)
        {
            auto mux = NamedMutex::openExisting(name);
            std::unique_lock<NamedMutex> lock(mux);

            auto mem = _utl::SharedMemoryRegion::openExisting(name, memSize, _utl::SharedMemoryRegion::AccessMode::ReadWrite);
            auto spinbuf = static_cast<LockingBuffer*>(mem.data());

            return CircularBufferOnSharedMemory(std::move(mem), spinbuf);
        }
        uint32_t write(const void * data, uint32_t size) override {
            std::unique_lock<decltype(spinbuf->spin)> lock(spinbuf->spin);
            auto l = spinbuf->buf._utl::CircularBuffer::write(data, size);
            return l;
        }
        uint32_t read(void * data, uint32_t maxSize) override {
            std::unique_lock<decltype(spinbuf->spin)> lock(spinbuf->spin);
            auto l = spinbuf->buf._utl::CircularBuffer::read(data, maxSize);
            return l;
        }
        bool flush() override { return true; }
        uint32_t capacity() const { return spinbuf->buf.capacity(); }
        uint32_t size() const { return spinbuf->buf.size(); }
    };

}