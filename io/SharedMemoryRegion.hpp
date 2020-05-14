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
    public:
        class Exception : public std::system_error {
        public:
            Exception(std::error_code err, const char * const msg) : std::system_error(err, msg) {}
            virtual ~Exception() {}
        };
        class NameNotExistException : public Exception {
        public:
            NameNotExistException(std::error_code err, const char * const msg) : Exception(err, msg) {}
            virtual ~NameNotExistException() {}
        };
        class BadNameException : public Exception {
        public:
            BadNameException(std::error_code err, const char * const msg) : Exception(err, msg) {}
            virtual ~BadNameException() {}
        };
        class OutOfMemoryException : public Exception {
        public:
            OutOfMemoryException(std::error_code err, const char * const msg) : Exception(err, msg) {}
            virtual ~OutOfMemoryException() {}
        };
        class AccessDeniedException : public Exception {
        public:
            AccessDeniedException(std::error_code err, const char * const msg) : Exception(err, msg) {}
            virtual ~AccessDeniedException() {}
        };
    private:
        #if defined(_WIN32)
        static void throwExceptionFromNativeErrorCode()
        {
            auto err = GetLastError();
            switch (err)
            {
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND: throw NameNotExistException(err, "Name does not exist."); break;
                case ERROR_ACCESS_DENIED: throw AccessDeniedException(err, "Access denied."); break;
                case ERROR_NOT_ENOUGH_MEMORY: throw OutOfMemoryException(err, "OS has not enough memory."); break;
                default:
                    throw Exception(err, "Construction failed. See native error code."); break;
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
            if (!h) { throwExceptionFromNativeErrorCode(); }
            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromNativeErrorCode(); }
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
            if (!h) { throwExceptionFromNativeErrorCode(); }

            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromNativeErrorCode(); }
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