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

#if defined(UTL_LOG_SHARED_MEMORY_REGION)

#include <utl/io/StdStreamWriter.hpp>
#include <utl/diagnostics/log-formatters/plaintextformatter.hpp>
#include <utl/diagnostics/logging.h>

struct StaticCtor
{
    StaticCtor()
    {
        static _utl::StdStreamWriter wt{&std::cerr};
        static _utl::PlainTextEventFormatter fmt;
        _utl::Logger::setGlobalEventLog(&fmt, &wt);
        _utl::Logger::setFlushFreq(_utl::SeverityLevel::Spam, 1);
    }
};
#define UTL_SHARED_MEMORY_REGION_LOG(msg, ...) UTL_logev(Spam, "[$f - $l:$m; $t] " msg ,##__VA_ARGS__)

#else

#define UTL_SHARED_MEMORY_REGION_LOG(msg, ...)

#endif

namespace _utl
{

    class SharedMemoryRegion {
    public:
        class Exception : public std::system_error {
        public:
            Exception(std::error_code err, const char * const msg) : std::system_error(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~Exception() {}
        };
        class NameNotExistException : public Exception {
        public:
            NameNotExistException(std::error_code err, const char * const msg) : Exception(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~NameNotExistException() {}
        };
        class NameAlreadyExistException : public Exception {
        public:
            NameAlreadyExistException(std::error_code err, const char * const msg) : Exception(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~NameAlreadyExistException() {}
        };
        class BadNameException : public Exception {
        public:
            BadNameException(std::error_code err, const char * const msg) : Exception(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~BadNameException() {}
        };
        class OutOfMemoryException : public Exception {
        public:
            OutOfMemoryException(std::error_code err, const char * const msg) : Exception(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~OutOfMemoryException() {}
        };
        class AccessDeniedException : public Exception {
        public:
            AccessDeniedException(std::error_code err, const char * const msg) : Exception(err, msg) { UTL_SHARED_MEMORY_REGION_LOG("exc.msg.:$ZS", msg); }
            virtual ~AccessDeniedException() {}
        };
    private:
        #if defined(_WIN32)
        static void throwExceptionFromNativeErrorCode()
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
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
        static void throwExceptionFromNativeErrorCode()
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
            int err = errno;
            #define ERR std::error_code{err,std::system_category()}
            UTL_SHARED_MEMORY_REGION_LOG("");
            switch (err)
            {
                case MAP_DENYWRITE:
                case EPERM:
                case EACCES: throw AccessDeniedException(ERR, "Permission was denied to shm_open() name in the specified mode, or O_TRUNC was specified and the caller does not have write permission on the object."); break;
                case ENOENT: throw NameNotExistException(ERR, "An attempt was made to shm_open() a name that did not exist, and O_CREAT was not specified."); break;
                case EMFILE: throw Exception(ERR, "The per-process limit on the number of open file descriptors has been reached."); break;
                case ENFILE: throw Exception(ERR, "The system-wide limit on the total number of open files has been reached."); break;
                case EEXIST: throw NameAlreadyExistException(ERR, "Both O_CREAT and O_EXCL were specified to shm_open() and the shared memory object specified by name already exists.");
                case EINVAL: throw BadNameException(ERR, "The name argument to shm_open() was invalid."); break;
                case ENAMETOOLONG: throw BadNameException(ERR, "The length of name exceeds PATH_MAX."); break;
                case EOVERFLOW:
                case ENOMEM: throw OutOfMemoryException(ERR, "No memory is available. Or: the process's maximum number of mappings would have been exceeded."); break;
                default: throw Exception(ERR, "Construction failed. See native error code."); break;
            }
            #undef ERR
        }
        #define CPY_ID(s) s
        #define DEL_ID(s) s
        using RegionId = int;
        #endif
        std::string nm;
        void * viewPtr;
        RegionId regionId;
        uint32_t viewSize;

        SharedMemoryRegion(RegionId hdl, void *ptr, uint32_t size, std::string name = "")
            : viewSize(size)
            , viewPtr(ptr)
            , nm(std::move(name))
            , regionId(hdl)
        { UTL_SHARED_MEMORY_REGION_LOG(""); }
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
            if (viewPtr) { UTL_SHARED_MEMORY_REGION_LOG("unmaping"); UnmapViewOfFile(viewPtr); }
            if (regionId) { UTL_SHARED_MEMORY_REGION_LOG("closing"); CloseHandle(regionId); }
            #elif defined(__linux__)
            if (viewPtr) { UTL_SHARED_MEMORY_REGION_LOG("unmaping"); munmap(viewPtr, viewSize); }
            if (regionId) { UTL_SHARED_MEMORY_REGION_LOG("closing"); close(regionId); }
            if (nm.size() > 0) { shm_unlink(nm.c_str()); }
            #endif
            DEL_ID(regionId);
        }
        SharedMemoryRegion()
            : SharedMemoryRegion(0, 0, 0, "")
        {}
        SharedMemoryRegion(SharedMemoryRegion && rhs)
            : viewPtr(rhs.viewPtr)
            , viewSize(rhs.viewSize)
            , regionId(rhs.regionId)
            , nm(std::move(rhs.nm))
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
            memset(&rhs, 0, sizeof(rhs));
        }
        SharedMemoryRegion(const SharedMemoryRegion &) = delete;

        SharedMemoryRegion & operator=(SharedMemoryRegion && rhs)
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
            this->SharedMemoryRegion::~SharedMemoryRegion();
            viewPtr = rhs.viewPtr;
            viewSize = rhs.viewSize;
            regionId = rhs.regionId;
            nm = std::move(rhs.nm);
            memset(&rhs, 0000, sizeof(rhs));
            return *this;
        }
        SharedMemoryRegion & operator=(const SharedMemoryRegion &) = delete;

        static SharedMemoryRegion create(std::string name, uint32_t size, AccessMode access = AccessMode::ReadWrite)
        {
            UTL_SHARED_MEMORY_REGION_LOG("name='$ZS', size=$WU, access=$WU", name.data(), size, (uint32_t)access);
            #if defined(_WIN32)
            name = "Global\\" + name;
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, name.data());
            if (!h) { throwExceptionFromNativeErrorCode(); }
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromNativeErrorCode(); }
            UTL_SHARED_MEMORY_REGION_LOG("");
            return SharedMemoryRegion(h,p,size);
            #elif defined(__linux__)
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto h = shm_open(name.data(), O_CREAT | O_RDWR, 0666);
            if (h < 0) throwExceptionFromNativeErrorCode();
            UTL_SHARED_MEMORY_REGION_LOG("");
            ftruncate(h, size);
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto p = mmap(0, size, (int) access, MAP_SHARED, h, 0);
            if (p < 0) throwExceptionFromNativeErrorCode();
            UTL_SHARED_MEMORY_REGION_LOG("");
            return SharedMemoryRegion(h, p, size, name);
            #endif
        }
        static SharedMemoryRegion openExisting(std::string name, uint32_t size, AccessMode access = AccessMode::ReadWrite)
        {
            UTL_SHARED_MEMORY_REGION_LOG("");
            #if defined(_WIN32)
            name = "Global\\" + name;
            auto h = OpenFileMappingA((DWORD) access, false, name.data());
            if (!h) { throwExceptionFromNativeErrorCode(); }
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto p = MapViewOfFile(h, (DWORD) access, 0, 0, size);
            if (!p) { throwExceptionFromNativeErrorCode(); }
            UTL_SHARED_MEMORY_REGION_LOG("");
            return SharedMemoryRegion(h, p, size);
            #elif defined(__linux__)
            auto h = shm_open(name.data(), O_RDWR, 0666);
            if (h < 0) throwExceptionFromNativeErrorCode();
            UTL_SHARED_MEMORY_REGION_LOG("");
            auto p = mmap(0, size, (int) access, MAP_SHARED, h, 0);
            if (p < 0) throwExceptionFromNativeErrorCode();
            UTL_SHARED_MEMORY_REGION_LOG("");
            return SharedMemoryRegion(h, p, size, "");
            #endif
        }
        void * data() { return viewPtr; }
        size_t size() { return viewSize; }
    };

}

#undef DEL_ID
#undef CPY_ID
#undef UTL_SHARED_MEMORY_REGION_LOG
