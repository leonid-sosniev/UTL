#pragma once

#include <utl/diagnostics/logging.hpp>

#if defined(__linux__)

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#else

#include <WinSock2.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")

#endif

#include <atomic>

namespace _utl { namespace logging {

namespace
{    
    template<class T> struct Raii {
        T value;
        std::function<void(T&)> destructor;
        Raii()
            : value{}
            , destructor{}
        {}
        Raii(std::function<void(T&)> dtor, T && val = T{})
            : value{std::move(val)}
            , destructor{dtor}
        {}
        ~Raii() {
            if (destructor) destructor(value);
        }
        Raii(const Raii &) = delete;
        Raii(Raii && rhs)
            : value(std::move(rhs.value))
            , destructor(std::move(rhs.destructor))
        {}
        Raii &operator=(const Raii &) = delete;
        Raii &operator=(Raii && rhs) {
            if (destructor) destructor(value);
            value = std::move(rhs.value);
            destructor = std::move(rhs.destructor);
        }
    };

    class NullEventFormatter : public AbstractEventFormatter {
    public:
        void formatEventAttributes(AbstractWriter & wtr, const EventAttributes & attr) final override {
            throw std::logic_error{ "NullEventFormatter::formatEventAttributes() is not implemented (and shall not be)." };
        }
        void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg arg[]) final override {
            throw std::logic_error{ "NullEventFormatter::formatEvent() is not implemented (and shall not be)." };
        }
        ~NullEventFormatter() final override {}
    };
    class NullWriter : public AbstractWriter {
    public:
        uint32_t write(const void * data, uint32_t size) final override {
            throw std::logic_error{ "NullWriter::write() is not implemented (and shall not be)." };
        }
        bool flush() final override {
            throw std::logic_error{ "NullWriter::flush() is not implemented (and shall not be)." };
        }
        ~NullWriter() final override {}
    };

    class AtomicLock {
        volatile std::atomic_flag & flag;
    public:
        AtomicLock(volatile std::atomic_flag &f) : flag(f)
        {
            while (flag.test_and_set()) {}
        }
        ~AtomicLock() {
            flag.clear();
        }
    };

    class ExposedBuffer {
    private:
        using Sync = volatile std::atomic_flag;
        using Lock = AtomicLock;

        uint32_t     m_writePoint alignas(64);
        uint32_t     m_writableSize;
        mutable Sync m_syncWt;

        uint32_t     m_readPoint alignas(64);
        uint32_t     m_readableSize;
        mutable Sync m_syncRd;

        uint8_t * m_buf alignas(64);
        uint32_t  m_size;
    private:
        inline uint8_t * perform(uint32_t & P, uint32_t &availableSize, uint32_t size)
        {
            if (P + size <= m_size) {
                if (size <= availableSize) {
                    availableSize -= size;
                    register auto ret = &m_buf[P];
                    P += size;
                    return ret;
                } else {
                    return nullptr;
                }
            } else {
                auto availContiguous = availableSize - (m_size - P);
                if (size <= availContiguous) {
                    availableSize = availContiguous - size;
                    P = size;
                    return m_buf;
                } else {
                    return nullptr;
                }
            }
        }
    public:
        ~ExposedBuffer() {
            Lock lk1{m_syncWt};
            Lock lk2{m_syncRd};
            delete[] m_buf;
            m_size = m_readableSize = m_writableSize = m_readPoint = m_writePoint = 0;
            m_buf = nullptr;
        }
        ExposedBuffer(uint32_t size)
            : m_buf(new uint8_t[size])
            , m_size(size)
            , m_readPoint(0)
            , m_writePoint(0)
            , m_readableSize(0)
            , m_writableSize(size)
            , m_syncWt()
            , m_syncRd()
        {
        }
        inline uint32_t size() const {
            return m_size;
        }
        inline void write(const void * data, uint32_t size)
        {
            register uint8_t *P = nullptr;
            register uint32_t freeSizeDelta;
            do {
                Lock wLock{m_syncWt};
                register uint32_t oldFreeSize = m_writableSize;
                P = perform(m_writePoint, m_writableSize, size);
                freeSizeDelta = oldFreeSize - m_writableSize;
            }
            while (P == nullptr);
            std::memcpy(P, data, size);

            Lock rLock{m_syncRd};
            m_readableSize += freeSizeDelta;
        }
        inline void read(void * data, uint32_t size)
        {
            register uint8_t * P = nullptr;
            do {
                Lock rLock{m_syncRd};
                P = perform(m_readPoint, m_readableSize, size);
            }
            while (P == nullptr);
            std::memcpy(data, P, size);
            clearExposed();
        }
        inline const void * tryExposeReceivedContiguousBlock(uint32_t size)
        {
            Lock rLock{m_syncRd};
            register uint8_t * P = perform(m_readPoint, m_readableSize, size);
            if (P == nullptr) return nullptr;
            return P;
        }
        inline void clearExposed()
        {
            Lock wLock{m_syncWt};
            Lock rLock{m_syncRd};
            m_writableSize = m_size - m_readableSize;
        }
    };
} // anonimous namespace

    class WebEventChannel : public AbstractEventChannel {
#if defined(__linux__)
        using SocketHandle = int;
#elif defined(_WIN32)
        struct SocketHandle {
            Raii<WSADATA> wsa;
            Raii<SOCKET> socket;
        };
#endif
        struct EventAttributesHolder {
            EventAttributes attr; // EventAttributes has no default constructor that's why the EventAttributesHolder is used in std::unordered_map
            char * stringsBuffer;
            EventAttributesHolder() : attr{"","","",0,0,0}, stringsBuffer(nullptr) {}
            ~EventAttributesHolder() { delete[] stringsBuffer; }
        };
        SocketHandle m_hdl;
        std::unordered_map<EventID,EventAttributesHolder> m_locations;
        std::vector<char> m_argsCache;
        static const uintptr_t ATTR_MARK = 0xAA115511BB0011EEull;
        static const uintptr_t OCCU_MARK = 0x00CC0055EE44CCEEull;
#if defined(__linux__)
        inline uint32_t os_write(const void * data, uint32_t size) {
            return ::write(m_hdl, data, size);
        }
        inline uint32_t os_read(void * data, uint32_t size) {
            return ::read(m_hdl, p + done, size - done);
        }
#elif defined(_WIN32)
        inline uint32_t os_write(const void * data, uint32_t size) {
            return ::send(m_hdl.socket.value, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
        }
        inline uint32_t os_read(void * data, uint32_t size) {
            return ::recv(m_hdl.socket.value, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
        }
#else
        #error WebEventChannel does not support the platform
#endif
        static SocketHandle os_createSender(uint16_t port, uint32_t ipAddr) {
#if defined(__linux__)
            sockaddr_in in;
            in.sin_family = AF_INET;
            in.sin_port = htons(port);
            in.sin_addr.s_addr = ipAddr;

            int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                throw std::runtime_error{ ::strerror(errno) };
            }
            int ret = ::connect(sock, (sockaddr*) &in, sizeof(in));
            if (ret < 0) switch (errno)
            {
                case ECONNREFUSED: throw std::runtime_error{ "Nobody listens on that end." };
                case EADDRINUSE:
                case EADDRNOTAVAIL: throw std::runtime_error{ "Address in use." };
                case EPERM:
                case EACCES:
                case ETIMEDOUT:
                case ENETUNREACH: throw std::runtime_error{ "Access denied." };
                case EBADF:
                case EFAULT:
                case ENOTSOCK:
                case EPROTOTYPE:
                case EAFNOSUPPORT: std::abort(); /* incorrect code */ break;
            }
            return sock;
#elif defined(_WIN32)
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.S_un.S_addr = ipAddr;

            Raii<WSADATA> wsa{
                [](WSADATA&){ ::WSACleanup(); }
            };
            if (::WSAStartup(MAKEWORD(2,2), &wsa.value) != 0) {
                throw std::runtime_error{ "WinSock2 initialization failed." };
            }

            Raii<SOCKET> socket{
                [](SOCKET & h) { ::closesocket(h); h = INVALID_SOCKET; },
                ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
            };
            if (socket.value == INVALID_SOCKET) {
                throw std::runtime_error{ "socket failed with error:" };
            }
            if (::connect(socket.value,(sockaddr*)&addr,sizeof(addr)) == SOCKET_ERROR) {
                auto err = GetLastError();
                return {};
                throw std::runtime_error{ std::to_string(err) };
            }
            return SocketHandle{
                std::move(wsa),
                std::move(socket)
            };
#endif
        }
        static SocketHandle os_createReceiver(uint16_t port, uint32_t ipAddr) {
#if defined(__linux__)
            sockaddr_in in;
            in.sin_family = AF_INET;
            in.sin_port = htons(port);
            in.sin_addr.s_addr = ipAddr;

            socklen_t l = sizeof(in);
            int ret = 0;

            int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                throw std::runtime_error{ ::strerror(errno) };
            }
            ret = ::bind(sock, (sockaddr*) &in, l);
            if (ret < 0) {
                throw std::runtime_error{ ::strerror(errno) };
            }
            ret = ::listen(sock, 1);
            if (ret < 0) {
                throw std::runtime_error{ ::strerror(errno) };
            }
            ret = ::accept(sock, (sockaddr*) &in, &l);
            if (ret < 0) {
                throw std::runtime_error{ ::strerror(errno) };
            }
            ::close(sock);
            return ret;
#elif defined(_WIN32)
            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.S_un.S_addr = ipAddr;
            
            Raii<WSADATA> wsa{
                [](WSADATA&) { ::WSACleanup(); }
            };
            if (WSAStartup(MAKEWORD(2,2), &wsa.value) != 0) {
                throw std::runtime_error{"WSAStartup failed with error: %d\n"};
            }

            Raii<SOCKET> listenSock{
                [](SOCKET & ls) { ::closesocket(ls); ls = INVALID_SOCKET; },
                ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
            };
            if (listenSock.value == INVALID_SOCKET) {
                return {};
            }
            if (::bind(listenSock.value, (sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
                return {};
            }
            if (::listen(listenSock.value, SOMAXCONN) == SOCKET_ERROR) {
                return {};
            }

            Raii<SOCKET> connSocket{
                [](SOCKET &s) { ::closesocket(s); s = INVALID_SOCKET; },
                ::accept(listenSock.value, NULL, NULL)
            };
            if (connSocket.value == INVALID_SOCKET) {
                return {};
            }
            return SocketHandle{
                std::move(wsa),
                std::move(connSocket)
            };
#endif
        }
    public:
        WebEventChannel(AbstractEventFormatter & formatter, AbstractWriter & writer, SocketHandle hdl)
            : AbstractEventChannel(formatter, writer)
            , m_hdl(std::move(hdl))
        {}
        ~WebEventChannel()
        {}
        static std::unique_ptr<WebEventChannel> createSender(uint16_t port, uint32_t ipAddr)
        {
            static NullEventFormatter fmt;
            static NullWriter wtr;
            auto sock = os_createSender(port, ipAddr);
            auto p = new WebEventChannel{fmt, wtr, std::move(sock)};
            std::unique_ptr<WebEventChannel> web{p};
            return std::move(web);
        }
        static std::unique_ptr<WebEventChannel> createReceiver(AbstractEventFormatter & formatter, AbstractWriter & writer, uint16_t port, uint32_t ipAddr)
        {
            auto sock = os_createReceiver(port, ipAddr);
            auto p = new WebEventChannel{formatter, writer, std::move(sock)};
            std::unique_ptr<WebEventChannel> web{p};
            return std::move(web);
        }
        static std::unique_ptr<WebEventChannel> createSender(uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return std::move(
                createSender(port, ipAddr[0] | (ipAddr[1] << 8) | (ipAddr[2] << 16) | (ipAddr[3] << 24))
            );
        }
        static std::unique_ptr<WebEventChannel> createReceiver(AbstractEventFormatter & formatter, AbstractWriter & writer, uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return std::move(
                createReceiver(formatter, writer, port, ipAddr[0] | (ipAddr[1] << 8) | (ipAddr[2] << 16) | (ipAddr[3] << 24))
            );
        }
        bool tryReceiveAndProcessEvent() final override {
            uintptr_t mark;
            uint32_t len[3];
            os_read(&mark, sizeof(void*));
            switch (mark) {
                default:
                    throw std::logic_error{ "" };
                case ATTR_MARK: {
                    EventID attrID;
                    os_read(&attrID, sizeof(attrID));
                    
                    auto &attrHolder = m_locations[attrID];
                    EventAttributes &attr = attrHolder.attr;

                    const_cast<EventID&>(attr.id) = attrID;

                    os_read((void*) &attr.line, sizeof(attr.line));
                    os_read((void*) &attr.argumentsExpected, sizeof(attr.argumentsExpected));

                    os_read(len, sizeof(len));
                    char *p = attrHolder.stringsBuffer = new char[len[0] + len[1] + len[2]];

                    #define RECV(str_end,size)\
                        const_cast<char*&>(str_end.str) = p;\
                        const_cast<char*&>(str_end.end) = (p += size);\
                        os_read(const_cast<char*>(str_end.str), size);
                    RECV(attr.messageFormat, len[0]);
                    RECV(attr.function,      len[1]);
                    RECV(attr.file,          len[2]);
                    #undef RECV
                    m_formatter.formatEventAttributes(m_writer, attr);
                    return tryReceiveAndProcessEvent();
                }
                case OCCU_MARK: {
                    typename std::remove_const<decltype(EventAttributes::argumentsExpected)>::type argCount = 0;
                    EventID attrID;
                    os_read(&attrID, sizeof(attrID));

                    os_read(&argCount, sizeof(argCount));
                    std::unique_ptr<Arg[]> args = std::make_unique<Arg[]>(argCount);
                    os_read(&args[0], sizeof(Arg) * argCount);

                    uint32_t argsTotalLength = 0;
                    for (uint32_t i = 0; i < argCount; ++i) {
                        register auto isArr = bool(args[i].type & Arg::TypeID::__ISARRAY);
                        if (isArr) {
                            argsTotalLength += Arg::typeSize(args[i].type) * args[i].arrayLength;
                        }
                    }
                    m_argsCache.resize(argsTotalLength);
                    uint32_t argsUsedLength = 0;
                    for (uint32_t i = 0; i < argCount; ++i) {
                        register auto isArr = bool(args[i].type & Arg::TypeID::__ISARRAY);
                        if (isArr) {
                            auto len = Arg::typeSize(args[i].type) * args[i].arrayLength;
                            args[i].valueOrArray.ArrayPointer = &m_argsCache[argsUsedLength];
                            argsUsedLength += len;
                            os_read((void*) args[i].valueOrArray.ArrayPointer, len);
                        }
                    }
                    m_formatter.formatEvent(m_writer, m_locations[attrID].attr, &args[0]);
                    return true;
                }
            }
        }
    private:
        void sendEventAttributes_(const EventAttributes & attr) final override {
            os_write(&ATTR_MARK, sizeof(void*));
            os_write(&attr.id,      sizeof(attr.id));
            os_write(&attr.line,  sizeof(attr.line));
            os_write(&attr.argumentsExpected, sizeof(attr.argumentsExpected));
            uint32_t len[3] = {
                uint32_t(attr.messageFormat.end - attr.messageFormat.str),
                uint32_t(attr.function.end      - attr.function.str),
                uint32_t(attr.file.end          - attr.file.str)
            };
            os_write(&len[0], sizeof(len));
            os_write(attr.messageFormat.str, len[0]);
            os_write(attr.function.str,      len[1]);
            os_write(attr.file.str,          len[2]);
        }
        void sendEventOccurrence_(const EventAttributes & attr, const Arg args[]) final override {
            os_write(&OCCU_MARK, sizeof(void*));
            os_write(&attr.id, sizeof(attr.id));
            os_write(&attr.argumentsExpected, sizeof(attr.argumentsExpected));
            os_write(args, sizeof(Arg) * attr.argumentsExpected);
            for (uint32_t i = 0; i < attr.argumentsExpected; ++i) {
                register auto isArr = bool(args[i].type & Arg::TypeID::__ISARRAY);
                if (isArr) {
                    os_write(args[i].valueOrArray.ArrayPointer, Arg::typeSize(args[i].type) * args[i].arrayLength);
                }
            }
        }
    };

} // namespace logging
} // namespace _utl
