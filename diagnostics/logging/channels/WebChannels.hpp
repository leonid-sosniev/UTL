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

#include <cassert>

#define nameof(identifier) #identifier

namespace _utl { namespace logging {

namespace {

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
            throw std::logic_error{ nameof(NullEventFormatter::formatEventAttributes()) " is not implemented (and shall not be)." };
        }
        void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg arg[]) final override {
            throw std::logic_error{ nameof(NullEventFormatter::formatEvent()) " is not implemented (and shall not be)." };
        }
        ~NullEventFormatter() final override {}
    };
    class NullTelemetryFormatter : public AbstractTelemetryFormatter {
    public:
        virtual void formatExpectedTypes(AbstractWriter & wtr, uint16_t count, const Arg::TypeID types[]) final override {
            throw std::logic_error{ nameof(NullTelemetryFormatter::formatExpectedTypes()) " is not implemented (and shall not be)." };
        }
        virtual void formatValues(AbstractWriter & wtr, const Arg arg[]) final override {
            throw std::logic_error{ nameof(NullTelemetryFormatter::formatValues()) " is not implemented (and shall not be)." };
        }
        ~NullTelemetryFormatter() final override {}
    };
    class NullWriter : public AbstractWriter {
    public:
        uint32_t write(const void * data, uint32_t size) final override {
            throw std::logic_error{ nameof(NullWriter::write()) " is not implemented (and shall not be)." };
        }
        bool flush() final override {
            throw std::logic_error{ nameof(NullWriter::flush()) " is not implemented (and shall not be)." };
        }
        ~NullWriter() final override {}
    };

    /**
     * @brief The class incapsulates web IO for few platforms
     */
    class WebIO {
    private:
#if defined(__linux__)
        using SocketHandle = int;
#elif defined(_WIN32)
        struct SocketHandle { Raii<WSADATA> wsa; Raii<SOCKET> socket; };
#else
        #error WebEventChannel does not support the platform
#endif
        SocketHandle m_hdl;
    private:
        WebIO(SocketHandle && handle) : m_hdl(std::move(handle)) {}
    public:
        explicit WebIO() : m_hdl()
        {}
        static inline uint32_t convIPArrayToIPNumber(const uint8_t (&ipAddr)[4])
        {
            return ipAddr[0] | (ipAddr[1] << 8) | (ipAddr[2] << 16) | (ipAddr[3] << 24);
        }
        static WebIO createSender(uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return createSender(port, convIPArrayToIPNumber(ipAddr));
        }
        static WebIO createSender(uint16_t port, uint32_t ipAddr) {
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
                case ECONNREFUSED: throw std::runtime_error{ nameof(WebIO::createSender) ": Nobody listens on that end." };
                case EADDRINUSE:
                case EADDRNOTAVAIL: throw std::runtime_error{ nameof(WebIO::createSender) ": Address in use." };
                case EPERM:
                case EACCES:
                case ETIMEDOUT:
                case ENETUNREACH: throw std::runtime_error{ nameof(WebIO::createSender) ": Access denied." };
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
                throw std::runtime_error{ nameof(WebIO::createSender) ": WinSock2 initialization failed." };
            }

            Raii<SOCKET> socket{
                [](SOCKET & h) { ::closesocket(h); h = INVALID_SOCKET; },
                ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
            };
            if (socket.value == INVALID_SOCKET) {
                throw std::runtime_error{ nameof(WebIO::createSender) ": socket failed with an error." };
            }
            if (::connect(socket.value,(sockaddr*)&addr,sizeof(addr)) == SOCKET_ERROR) {
                auto err = GetLastError();
                throw std::runtime_error{ nameof(WebIO::createSender) ": Failed to connect to the socket with the error " + std::to_string(err) };
            }
            return SocketHandle{
                std::move(wsa),
                std::move(socket)
            };
#endif
        }
        static WebIO createReceiver(uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return createReceiver(port, convIPArrayToIPNumber(ipAddr));
        }
        static WebIO createReceiver(uint16_t port, uint32_t ipAddr) {
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
                throw std::runtime_error{nameof(WebIO::createReceiver) ": WSAStartup failed with error: %d\n"};
            }

            Raii<SOCKET> listenSock{
                [](SOCKET & ls) { ::closesocket(ls); ls = INVALID_SOCKET; },
                ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
            };
            if (listenSock.value == INVALID_SOCKET) {
                return SocketHandle{};
            }
            if (::bind(listenSock.value, (sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
                return SocketHandle{};
            }
            if (::listen(listenSock.value, SOMAXCONN) == SOCKET_ERROR) {
                return SocketHandle{};
            }

            Raii<SOCKET> connSocket{
                [](SOCKET &s) { ::closesocket(s); s = INVALID_SOCKET; },
                ::accept(listenSock.value, NULL, NULL)
            };
            if (connSocket.value == INVALID_SOCKET) {
                return SocketHandle{};
            }
            return SocketHandle{
                std::move(wsa),
                std::move(connSocket)
            };
#endif
        }
        inline uint32_t write(const void * data, uint32_t size)
        {
#if defined(__linux__)
            return ::write(m_hdl, data, size);
#elif defined(_WIN32)
            return ::send(m_hdl.socket.value, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
#endif
        }
        inline uint32_t read(void * data, uint32_t size)
        {
#if defined(__linux__)
            return ::read(m_hdl, p + done, size - done);
#elif defined(_WIN32)
            return ::recv(m_hdl.socket.value, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
#endif
        }
    };

} // anonimous namespace

    class WebEventChannel : public AbstractEventChannel {
        struct EventAttributesHolder {
            EventAttributes attr; // EventAttributes has no default constructor that's why the EventAttributesHolder is used in std::unordered_map
            char * stringsBuffer;
            EventAttributesHolder() : attr{"","","",0,0,0}, stringsBuffer(nullptr) {}
            ~EventAttributesHolder() { delete[] stringsBuffer; }
        };
        WebIO m_web;
        std::unordered_map<EventID,EventAttributesHolder> m_locations;
        std::vector<char> m_argsCache;
        static const uintptr_t ATTR_MARK = 0xAA115511BB0011EEull;
        static const uintptr_t OCCU_MARK = 0x00CC0055EE44CCEEull;
    public:
        WebEventChannel(uint32_t argsAllocatorCapacity, AbstractEventFormatter & formatter, AbstractWriter & writer, WebIO && web)
            : AbstractEventChannel(formatter, writer, argsAllocatorCapacity)
            , m_web(std::move(web))
        {}
        ~WebEventChannel()
        {}
        static std::unique_ptr<WebEventChannel> createSender(uint32_t argsAllocatorCapacity, uint16_t port, uint32_t ipAddr)
        {
            static NullEventFormatter fmt;
            static NullWriter wtr;
            auto sock = WebIO::createSender(port, ipAddr);
            auto p = new WebEventChannel{argsAllocatorCapacity, fmt, wtr, std::move(sock)};
            std::unique_ptr<WebEventChannel> web{p};
            return std::move(web);
        }
        static std::unique_ptr<WebEventChannel> createReceiver(AbstractEventFormatter & formatter, AbstractWriter & writer, uint16_t port, uint32_t ipAddr)
        {
            auto sock = WebIO::createReceiver(port, ipAddr);
            auto p = new WebEventChannel{0, formatter, writer, std::move(sock)};
            std::unique_ptr<WebEventChannel> web{p};
            return std::move(web);
        }
        static std::unique_ptr<WebEventChannel> createSender(uint32_t argsAllocatorCapacity, uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return std::move(
                createSender(argsAllocatorCapacity, port, WebIO::convIPArrayToIPNumber(ipAddr))
            );
        }
        static std::unique_ptr<WebEventChannel> createReceiver(AbstractEventFormatter & formatter, AbstractWriter & writer, uint16_t port, const uint8_t (&ipAddr)[4])
        {
            return std::move(
                createReceiver(formatter, writer, port, WebIO::convIPArrayToIPNumber(ipAddr))
            );
        }
        bool tryReceiveAndProcessEvent() final override {
            uintptr_t mark;
            uint32_t len[3];
            m_web.read(&mark, sizeof(void*));
            switch (mark) {
                default:
                    throw std::logic_error{ "" };
                case ATTR_MARK: {
                    EventID attrID;
                    m_web.read(&attrID, sizeof(attrID));
                    
                    auto &attrHolder = m_locations[attrID];
                    EventAttributes &attr = attrHolder.attr;

                    const_cast<EventID&>(attr.id) = attrID;

                    m_web.read((void*) &attr.line, sizeof(attr.line));
                    m_web.read((void*) &attr.argumentsExpected, sizeof(attr.argumentsExpected));

                    m_web.read(len, sizeof(len));
                    char *p = attrHolder.stringsBuffer = new char[len[0] + len[1] + len[2]];

                    #define RECV(str_end,size)\
                        const_cast<char*&>(str_end.str) = p;\
                        const_cast<char*&>(str_end.end) = (p += size);\
                        m_web.read(const_cast<char*>(str_end.str), size);
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
                    m_web.read(&attrID, sizeof(attrID));

                    m_web.read(&argCount, sizeof(argCount));
                    std::unique_ptr<Arg[]> args = std::make_unique<Arg[]>(argCount);
                    m_web.read(&args[0], sizeof(Arg) * argCount);

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
                            m_web.read((void*) args[i].valueOrArray.ArrayPointer, len);
                        }
                    }
                    m_formatter.formatEvent(m_writer, m_locations[attrID].attr, &args[0]);
                    return true;
                }
            }
        }
    private:
        void sendEventAttributes_(const EventAttributes & attr) final override {
            m_web.write(&ATTR_MARK, sizeof(void*));
            m_web.write(&attr.id,      sizeof(attr.id));
            m_web.write(&attr.line,  sizeof(attr.line));
            m_web.write(&attr.argumentsExpected, sizeof(attr.argumentsExpected));
            uint32_t len[3] = {
                uint32_t(attr.messageFormat.end - attr.messageFormat.str),
                uint32_t(attr.function.end      - attr.function.str),
                uint32_t(attr.file.end          - attr.file.str)
            };
            m_web.write(&len[0], sizeof(len));
            m_web.write(attr.messageFormat.str, len[0]);
            m_web.write(attr.function.str,      len[1]);
            m_web.write(attr.file.str,          len[2]);
        }
        void sendEventOccurrence_(const EventAttributes & attr, const Arg args[]) final override {
            m_web.write(&OCCU_MARK, sizeof(void*));
            m_web.write(&attr.id, sizeof(attr.id));
            m_web.write(&attr.argumentsExpected, sizeof(attr.argumentsExpected));
            m_web.write(args, sizeof(Arg) * attr.argumentsExpected);
            for (uint32_t i = 0; i < attr.argumentsExpected; ++i) {
                register auto isArr = bool(args[i].type & Arg::TypeID::__ISARRAY);
                if (isArr) {
                    m_web.write(args[i].valueOrArray.ArrayPointer, Arg::typeSize(args[i].type) * args[i].arrayLength);
                }
            }
            releaseArgs(attr.argumentsExpected);
        }
    };

    class WebTelemetryChannel : public AbstractTelemetryChannel {
    private:
        static NullTelemetryFormatter m_fmt;
        static NullWriter m_wtr;
        enum class EChannelEnd { Sender, Receiver };
        WebIO m_web;
        Arg::TypeID * m_types;
        Arg * m_sample;
        Arg m_sampleMark[1];
        uint16_t m_sampleLength;
        const EChannelEnd m_channelEnd;
    private:
        WebTelemetryChannel(EChannelEnd end, WebIO && web, AbstractTelemetryFormatter & fmt, AbstractWriter & wtr, uint32_t argsAllocatorCapacity, uint16_t sampleLength, const Arg::TypeID types[])
            : AbstractTelemetryChannel(fmt, wtr, argsAllocatorCapacity)
            , m_web(std::move(web))
            , m_types(nullptr)
            , m_sample(&m_sampleMark[0])
            , m_sampleLength(sampleLength)
            , m_channelEnd(end)
        {
            initializeAfterConstruction(sampleLength, types);
        }
        void readFull(void * data, uint16_t size)
        {
            assert(m_channelEnd == EChannelEnd::Receiver);
            register auto p = static_cast<uint8_t*>(data);
            register auto end = p + size;
            while (p < end)
            {
                p += m_web.read(p, end - p);
            }
        }
        void writeJunk(uint32_t size)
        {
            assert(m_channelEnd == EChannelEnd::Sender);
            static const Arg junk{
                Arg::Value{nullptr},
                Arg::TypeID::TI_NONE,
                0
            };
            while (size >= sizeof(Arg)) {
                size -= sizeof(Arg);
                m_web.write(&junk, sizeof(Arg));
            }
            m_web.write(&junk, size);
        }
        void readJunk(uint32_t size)
        {
            Arg junk;
            while (size >= sizeof(Arg)) {
                size -= sizeof(Arg);
                readFull(&junk, sizeof(Arg));
                if (junk.valueOrArray.ArrayPointer || (int)junk.type || junk.arrayLength) {
                    throw std::logic_error{""};
                }
            }
            readFull(&junk, size);
            if (junk.valueOrArray.ArrayPointer || (int)junk.type || junk.arrayLength) {
                return;
            }
        }
        virtual void sendSampleTypes_(uint16_t sampleLength, const Arg::TypeID sampleTypes[]) final override
        {
            if (sampleLength == 0) {
                /** We are in the receiver channel end */
                return;
            }
            assert(m_channelEnd == EChannelEnd::Sender);

            auto typesSize = sizeof(Arg::TypeID) * sampleLength;
            auto sampleSize = sizeof(Arg) * sampleLength;

            Arg markerArg{
                Arg::Value{ (void*) 0xFAFAFAFA },
                Arg::TypeID::__TYPE_ID_COUNT,
                sampleLength
            };

            m_web.write(&markerArg, sizeof(Arg));
            m_web.write(sampleTypes, typesSize);
        }
        virtual void sendSample_(const Arg values[]) final override
        {
            assert(m_channelEnd == EChannelEnd::Sender);

            m_web.write(values, sizeof(Arg) * m_sampleLength);
            for (uint16_t i = 0; i < m_sampleLength; ++i)
            {
                register const Arg & arg = values[i];
                if (arg.type & Arg::TypeID::__ISARRAY)
                {
                    auto size = Arg::typeSize(arg.type) * arg.arrayLength;
                    m_web.write(arg.valueOrArray.ArrayPointer, size);
                }
            }
        }
    public:
        WebTelemetryChannel(WebTelemetryChannel && rhs)
            : AbstractTelemetryChannel(std::move(rhs))
            , m_web(std::move(rhs.m_web))
            , m_types(rhs.m_types)
            , m_sample(rhs.m_sample)
            , m_sampleLength(rhs.m_sampleLength)
            , m_channelEnd(rhs.m_channelEnd)
        {
            rhs.m_types = nullptr;
            rhs.m_sample = nullptr;
            rhs.m_sampleLength = 0;
        }
        ~WebTelemetryChannel()
        {
            delete[] m_types;
            delete[] m_sample;
        }
        /** Constructs the receiver end of the channel */
        static WebTelemetryChannel createReceiver(uint16_t port, const uint8_t (&ipAddr)[4], AbstractTelemetryFormatter & fmt, AbstractWriter & wtr)
        {
            WebTelemetryChannel ret{
                EChannelEnd::Receiver,
                std::move(WebIO::createReceiver(port, ipAddr)),
                fmt, wtr,
                0,
                0, nullptr
            };
            ret.m_sampleLength = 1;
            ret.m_sample = &ret.m_sampleMark[0];
            return std::move(ret);
        }
        /** Constructs the sender end of the channel */
        static WebTelemetryChannel createSender(uint16_t port, const uint8_t (&ipAddr)[4], uint32_t argsAllocatorCapacity, uint16_t sampleLength, const Arg::TypeID types[])
        {
            return WebTelemetryChannel{
                EChannelEnd::Sender,
                std::move(WebIO::createSender(port, ipAddr)),
                m_fmt, m_wtr,
                argsAllocatorCapacity,
                sampleLength, types
            };
        }
        virtual uint16_t sampleLength() const final override
        {
            return m_sampleLength;
        }
        virtual bool tryProcessSample() final override
        {
            assert(m_channelEnd == EChannelEnd::Receiver);
            assert(m_sampleLength > 0);

            readFull(&m_sample[0], sizeof(Arg) * m_sampleLength);

            if (m_sample[0].type == Arg::TypeID::__TYPE_ID_COUNT && m_sample[0].valueOrArray.ArrayPointer == (void*) 0xFAFAFAFA)
            {
                m_sampleLength = m_sample[0].arrayLength;
                m_sample = new Arg[m_sampleLength];
                m_types = new Arg::TypeID[m_sampleLength];
                readFull(&m_types[0], sizeof(Arg::TypeID) * m_sampleLength);

                this->AbstractTelemetryChannel::m_formatter.formatExpectedTypes(this->AbstractTelemetryChannel::m_sink, m_sampleLength, m_types);

                auto succ = tryProcessSample();
                return succ;
            }
            else
            {
                for (uint16_t i = 0; i < m_sampleLength; ++i)
                {
                    register Arg & arg = m_sample[i];
                    if (arg.type & Arg::TypeID::__ISARRAY)
                    {
                        auto size = Arg::typeSize(arg.type) * arg.arrayLength;
                        readFull(const_cast<void*&>(arg.valueOrArray.ArrayPointer) = new char[size], size);
                    }
                }
                this->AbstractTelemetryChannel::m_formatter.formatValues(this->AbstractTelemetryChannel::m_sink, m_sample);
            }
            return true;
        }
    };
    NullTelemetryFormatter WebTelemetryChannel::m_fmt{};
    NullWriter WebTelemetryChannel::m_wtr{};

} // namespace logging
} // namespace _utl

#if defined(CATCH_CONFIG_MAIN)

#include <future>

TEST_CASE("WebTelemetryChannel smoke test", "[web][telemetry][channel]")
{
    using namespace _utl::logging;
    std::packaged_task<void()> producer{
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
            Arg::TypeID ids[3] = {
                Arg::TypeID::TI_arrayof_Char, Arg::TypeID::TI_i64, Arg::TypeID::TI_Thread
            };
            auto chan = WebTelemetryChannel::createSender(12, {127,0,0,1}, 1024, 3, ids);
            UTL_logsam(chan, "some text", (int64_t) -3, std::this_thread::get_id());
            UTL_logsam(chan, "some", (int64_t) -2, std::this_thread::get_id());
            UTL_logsam(chan, "text", (int64_t) 00, std::this_thread::get_id());
            UTL_logsam(chan, "`", (int64_t) 65536, std::this_thread::get_id());
        }
    };
    
    std::stringstream stream;
    PlainTextTelemetryFormatter fmt;
    StreamWriter wtr{stream};
    std::thread producerThread{ std::move(producer) };
    auto chanRecv = WebTelemetryChannel::createReceiver(12, {127,0,0,1}, fmt, wtr);

    REQUIRE(chanRecv.tryProcessSample());
    REQUIRE(chanRecv.tryProcessSample());
    REQUIRE(chanRecv.tryProcessSample());
    REQUIRE(chanRecv.tryProcessSample());

    producerThread.join();

    std::cout << stream.str() << std::endl;
}

#endif
