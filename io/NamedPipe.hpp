#pragma once

#include <windows.h>
#include <cctype>
#include <strsafe.h>
#include <cstdio>
#include <string>
#include <memory>
#include <utl/io/Reader.hpp>
#include <utl/io/Writer.hpp>

namespace _utl
{

    class NamedPipe : public _utl::AbstractReader, public _utl::AbstractWriter {
    public:
        enum AccessMode : DWORD
        {
            am_Read = PIPE_ACCESS_INBOUND,
            am_Write = PIPE_ACCESS_OUTBOUND,
            am_ReadWrite = PIPE_ACCESS_DUPLEX
        };
        struct Timeout
        {
            operator const uint32_t() const { return m_msecs; }

            static Timeout waitMilliseconds(uint32_t msec)
            {
                if (msec == uint32_t(-1) || msec == 0) { throw std::invalid_argument("0x00 and 0xFFFFFFFF are forbidden values"); }
                return msec;
            }
            static Timeout waitDefault() { return NMPWAIT_USE_DEFAULT_WAIT; }
            static Timeout waitForever() { return NMPWAIT_WAIT_FOREVER; }
        private:
            uint32_t m_msecs;
            Timeout(uint32_t _) : m_msecs(_) {}
        };
    private:
        NamedPipe(HANDLE h) : m_handle(h)
        {}
        HANDLE m_handle;
    private:
        static inline NamedPipe spawn(HANDLE handler)
        {
            if (handler == INVALID_HANDLE_VALUE)
            {
                throw std::invalid_argument("Cannot create thi pipe with invalid handle");
            }
            return NamedPipe(handler);
        }
        static inline std::string pipeName(const std::string & name)
        {
            return "\\\\.\\pipe\\" + name;
        }
        static inline DWORD toGenericAccessMode(AccessMode access)
        {
            switch (access)
            {
                case AccessMode::am_Read: return GENERIC_READ;
                case AccessMode::am_Write: return GENERIC_WRITE;
                case AccessMode::am_ReadWrite: return GENERIC_READ | GENERIC_WRITE;
                default: throw std::logic_error("incomplete switch operator");
            }
        }
    public:
        NamedPipe(NamedPipe && rhs) : m_handle(rhs.m_handle)
        {
            rhs.m_handle = 0;
        }
        NamedPipe(const NamedPipe &) = delete;

        NamedPipe & operator=(NamedPipe && rhs)
        {
            m_handle = rhs.m_handle;
            rhs.m_handle = 0;
        }
        NamedPipe & operator=(const NamedPipe &) = delete;

        ~NamedPipe() {
            if (m_handle) { CloseHandle(m_handle); }
        }

        static NamedPipe create(
            const std::string & name, AccessMode access,
            Timeout timeout = Timeout::waitDefault(),
            uint32_t readerBufferSize = 65536, uint32_t writerBufferSize = 65536)
        {
            auto nm = pipeName(name);
            auto h = CreateNamedPipeA(
                nm.data(), (DWORD) access, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1, writerBufferSize, readerBufferSize, timeout, NULL
            );
            return spawn(h);
        }
        static NamedPipe connectTo(
            const std::string & name, AccessMode access,
            Timeout timeout = Timeout::waitDefault())
        {
            auto nm = pipeName(name);

            while (WaitNamedPipeA(nm.data(), timeout) == false)
            {
                auto err = GetLastError();

                if (err == ERROR_SEM_TIMEOUT) {
                    return nullptr;
                }
                else if (err == ERROR_FILE_NOT_FOUND) {
                    throw std::invalid_argument("pipe does not exist");
                }
                else {
                    throw std::runtime_error("");
                }
            }
            auto h = CreateFileA(
                nm.data(), toGenericAccessMode(access), 0, NULL, OPEN_EXISTING, 0, NULL
            );
            return spawn(h);
        }
        bool waitForClient()
        {
            int sts = ConnectNamedPipe(m_handle, NULL);
            return sts ? true : GetLastError() == ERROR_PIPE_CONNECTED;
        }
        uint32_t write(const void * data, uint32_t size) override
        {
            DWORD count = 0;
            WriteFile(m_handle, data, size, &count, NULL);
            return count;
        }
        uint32_t read(void * data, uint32_t size) override
        {
            DWORD count = 0;
            ReadFile(m_handle, data, size, &count, NULL);
            return count;
        }
    };

}
