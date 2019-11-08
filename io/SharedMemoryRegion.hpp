#pragma once

#if defined(__linux__)
#   include <fcntl.h>
#   include <unistd.h>
#   include <sys/shm.h>
#   include <sys/mman.h>
#   include <sys/stat.h>
    using HANDLE = int;
#elif defined(_WIN32)
#   include <windows.h>
#   include <strsafe.h>
#else
#   error "Unsupported platforms"
#endif
#include <string.h>
#include <cctype>
#include <stdexcept>

namespace _utl
{

    class SharedMemoryRegion {
    private:
#if defined(_WIN32)
        static void throwExceptionFromWin32LastErrorCode()
        {
            auto err = GetLastError();
            switch (err)
            {
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND: throw std::runtime_error("Name does not exist."); break;
                case ERROR_ACCESS_DENIED: throw std::runtime_error("Acces denied."); break;
                case ERROR_NOT_ENOUGH_MEMORY: throw std::runtime_error("OS has not enough memory."); break;
                default: {
                    std::string msg; msg.reserve(64);
                    msg.append("Creation of shared memory segment failed. Win32 errcode: ");
                    msg.append(std::to_string(err));
                    msg.append(".");
                    throw std::runtime_error(msg);
                }
            }
        }
        #define CPY_ID(s) s
        #define DEL_ID(s)
        using RegionId = HANDLE;
#elif defined(__linux__)
        #define CPY_ID(s) strcpy(new char[s.size() + 1], s.data())
        #define DEL_ID(s) delete[] s
        using RegionId = const char *;
#endif
        RegionId regionId;
        void * viewPtr;
        uint32_t viewSize;

        SharedMemoryRegion(RegionId hdl, void *ptr, uint32_t size)
            : viewSize(size)
            , viewPtr(ptr)
            , regionId(hdl)
        {}
    public:
        enum class AccessMode {
#if defined(_WIN32)
            Read = FILE_MAP_READ, Write = FILE_MAP_WRITE, ReadWrite = FILE_MAP_ALL_ACCESS
#elif defined(__linux__)
            Read = PROT_READ, Write = PROT_WRITE, ReadWrite = PROT_WRITE | PROT_READ
#endif
        };

        ~SharedMemoryRegion()
        {
#if defined(_WIN32)
            if (viewPtr) { UnmapViewOfFile(viewPtr); }
            if (regionId) { CloseHandle(regionId); }
#elif defined(__linux__)
            if (viewPtr) { munmap(viewPtr, viewSize); }
            if (regionId) { shm_unlink(regionId); }
#endif
            DEL_ID(regionId);
        }
        SharedMemoryRegion()
            : SharedMemoryRegion(0, 0, 0)
        {}
        SharedMemoryRegion(SharedMemoryRegion && rhs)
            : viewPtr(rhs.viewPtr)
            , viewSize(rhs.viewSize)
            , regionId(rhs.regionId)
        {
            memset(&rhs, 0, sizeof(rhs));
        }
        SharedMemoryRegion(const SharedMemoryRegion &) = delete;

        SharedMemoryRegion & operator=(SharedMemoryRegion && rhs)
        {
            memcpy(this, &rhs, sizeof(rhs));
            memset(&rhs, 0000, sizeof(rhs));
            return *this;
        }
        SharedMemoryRegion & operator=(const SharedMemoryRegion &) = delete;

        static SharedMemoryRegion create(std::string name, uint32_t size, AccessMode access = AccessMode::ReadWrite)
        {
#if defined(_WIN32)
            name = "Global\\" + name;
            auto h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, name.data());
            if (!h) { throwExceptionFromWin32LastErrorCode(); }
            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromWin32LastErrorCode(); }
            return SharedMemoryRegion(h,p,size);
#elif defined(__linux__)
            auto h = shm_open(name.data(), O_CREAT | O_RDWR, 0666);
            ftruncate(h, size);
            auto p = mmap(0, size, (int) access, MAP_SHARED, h, 0);
            return SharedMemoryRegion(CPY_ID(name), p, size);
#endif
        }
        static SharedMemoryRegion openExisting(std::string name, uint32_t size, AccessMode access = AccessMode::ReadWrite)
        {
#if defined(_WIN32)
            name = "Global\\" + name;
            auto h = OpenFileMappingA((DWORD) access, false, name.data());
            if (!h) { throwExceptionFromWin32LastErrorCode(); }

            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromWin32LastErrorCode(); }
            return SharedMemoryRegion(h, p, size);
#elif defined(__linux__)
            auto h = shm_open(name.data(), O_RDWR, 0666);
            auto p = mmap(0, size, (int) access, MAP_SHARED, h, 0);
            return SharedMemoryRegion(CPY_ID(name), p, size);
#endif
        }
        void * data() { return viewPtr; }
        size_t size() { return viewSize; }
    };

}

#undef DEL_ID
#undef CPY_ID