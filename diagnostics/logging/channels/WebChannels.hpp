#pragma once

#include <utl/diagnostics/logging.hpp>
#include <cassert>
#include <stdexcept>

#if defined(__linux__)
  #include <sys/shm.h>
  #include <sys/mman.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <fcntl.h>
#elif defined(_WIN32)
  #include <WinSock2.h>
  #pragma comment (lib, "Ws2_32.lib")
  #pragma comment (lib, "Mswsock.lib")
#else
  #error WebEventChannel does not support the platform
#endif

namespace _utl { namespace logging {

namespace {

    enum EAddressFamily {
        IPv4 = AF_INET,
        APv6 = AF_INET6,
    };
    class IPAddress {
        friend class UDPSocket;
    private:
        sockaddr_in m_nativeAddr;
        EAddressFamily m_addrType;
    public:
        IPAddress(uint16_t port, const uint8_t (&ip)[4]) : m_addrType(EAddressFamily::IPv4)
        {
            std::memset(&m_nativeAddr, 0, sizeof(m_nativeAddr));
            m_nativeAddr.sin_family = AF_INET;
            m_nativeAddr.sin_port = htons(port);
          #if defined(__linux__)
            m_nativeAddr.sin_addr.s_addr = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
          #elif defined(_WIN32)
            m_nativeAddr.sin_addr.S_un.S_addr = ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
          #endif
        }
    };

    class UDPSocket {
    private:
        static void initNetLib() {
          #if defined(_WIN32)
            struct WinDataSingleton {
                WSADATA m_wsa;
                WinDataSingleton() { ::WSAStartup(MAKEWORD(2,2), &m_wsa); }
                ~WinDataSingleton() { ::WSACleanup(); }
            };
            static WinDataSingleton data;
          #endif
        }
    protected:
      #if defined(__linux__)
        using socket_handle_type = int;
      #elif defined(_WIN32)
        using socket_handle_type = SOCKET;
      #endif
        socket_handle_type m_validHandle;
    protected:
        static inline std::string getLastErrorAsString() {
          #if defined(__linux__)
            return ::strerror(errno)
          #elif defined(_WIN32)
            LPSTR buffer = nullptr;
            size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, NULL);
            std::string message(buffer, size);
            LocalFree(buffer);
            return std::move(message);
          #endif
        }
        static inline void throwOnError(int x) {
            if (x < 0) throw std::runtime_error{ getLastErrorAsString() };
        }
    public:
        ~UDPSocket() {
          #if defined(__linux__)
            ::close(m_validHandle);
          #elif defined(_WIN32)
            ::closesocket(m_validHandle);
          #endif
        }
        UDPSocket(UDPSocket && rhs) : m_validHandle(rhs.m_validHandle) {
            rhs.m_validHandle = -1;
        }
        UDPSocket(EAddressFamily addrFamily) {
            initNetLib();
            m_validHandle = ::socket(addrFamily, SOCK_DGRAM, IPPROTO_UDP);
            throwOnError(m_validHandle);
        }
        inline uint32_t write(const void * data, uint32_t size) {
            assert(m_validHandle >= 0);
          #if defined(__linux__)
            return ::write(m_validHandle, data, size);
          #elif defined(_WIN32)
            return ::send(m_validHandle, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
          #endif
        }
        inline uint32_t read(void * data, uint32_t size) {
            assert(m_validHandle >= 0);
          #if defined(__linux__)
            return ::read(m_validHandle, data, size);
          #elif defined(_WIN32)
            return ::recv(m_validHandle, (char*)data, size, 0) == SOCKET_ERROR ? 0 : size;
          #endif
        }
        bool bind(const IPAddress & ipAddr) {
            assert(m_validHandle >= 0);
            return 0 == ::bind(m_validHandle, (sockaddr*) &ipAddr.m_nativeAddr, sizeof(ipAddr.m_nativeAddr));
        }
        bool connect(const IPAddress & ipAddr) {
            assert(m_validHandle >= 0);
          #if defined(_WIN32)
            u_long enableNonblocking = 1;
            throwOnError(::ioctlsocket(m_validHandle, FIONBIO, &enableNonblocking));
          #endif
            return 0 == ::connect(m_validHandle, (sockaddr*) &ipAddr.m_nativeAddr, sizeof(ipAddr.m_nativeAddr));
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
    class NullTelemetryFormatter : public AbstractTelemetryFormatter {
    public:
        virtual void formatExpectedTypes(AbstractWriter & wtr, uint16_t count, const Arg::TypeID types[]) final override {
            throw std::logic_error{ "NullTelemetryFormatter::formatExpectedTypes() is not implemented (and shall not be)." };
        }
        virtual void formatValues(AbstractWriter & wtr, const Arg arg[]) final override {
            throw std::logic_error{ "NullTelemetryFormatter::formatValues() is not implemented (and shall not be)." };
        }
        ~NullTelemetryFormatter() final override {}
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

} // anonimous namespace

    class WebEventChannel : public AbstractEventChannel {
        struct EventAttributesHolder {
            EventAttributes attr; // EventAttributes has no default constructor that's why the EventAttributesHolder is used in std::unordered_map
            char * stringsBuffer;
            EventAttributesHolder() : attr{"","","",0,0,0}, stringsBuffer(nullptr) {}
            ~EventAttributesHolder() { delete[] stringsBuffer; }
        };
        UDPSocket m_web;
        std::unordered_map<EventID,EventAttributesHolder> m_locations;
        std::vector<char> m_argsCache;
        WebEventChannel(uint32_t argsAllocatorCapacity, AbstractEventFormatter & formatter, AbstractWriter & writer, UDPSocket && web)
            : AbstractEventChannel(formatter, writer, argsAllocatorCapacity)
            , m_web(std::move(web))
        {}
        static const uintptr_t ATTR_MARK = 0xAA115511BB0011EEull;
        static const uintptr_t OCCU_MARK = 0x00CC0055EE44CCEEull;
    public:
        ~WebEventChannel()
        {}
        static std::unique_ptr<WebEventChannel> createSender(uint32_t argsAllocatorCapacity, uint16_t port, const uint8_t (&ipAddr)[4])
        {
            static NullEventFormatter fmt;
            static NullWriter wtr;
            UDPSocket sock{EAddressFamily::IPv4};

            bool isConnected = sock.connect(IPAddress{port,ipAddr});
            if (isConnected) {
                std::unique_ptr<WebEventChannel> owning;
                owning.reset(new WebEventChannel{ argsAllocatorCapacity, fmt, wtr, std::move(sock) });
                return std::move(owning);
            } else {
                throw std::runtime_error{ "Failed to connect the socket." };
            }
        }
        static std::unique_ptr<WebEventChannel> createReceiver(AbstractEventFormatter & formatter, AbstractWriter & writer, uint16_t port, const uint8_t (&ipAddr)[4])
        {
            UDPSocket sock{EAddressFamily::IPv4};
            bool isBound = sock.bind(IPAddress{port,ipAddr});
            if (isBound) {
                std::unique_ptr<WebEventChannel> owning;
                owning.reset(new WebEventChannel{ 0, formatter, writer, std::move(sock) });
                return std::move(owning);
            } else {
                throw std::runtime_error{ "Failed to bind the socket to an IP address." };
            }
        }
        bool tryReceiveAndProcessEvent() final override
        {
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
        UDPSocket m_web;
        Arg::TypeID * m_receivedTypes;
        Arg * m_receivedSample;
        Arg m_sampleHeadBuffer[1];
        uint16_t m_sampleLength;
        const EChannelEnd m_channelEnd;
    private:
        WebTelemetryChannel(EChannelEnd end, UDPSocket && web, AbstractTelemetryFormatter & fmt, AbstractWriter & wtr, uint32_t argsAllocatorCapacity, uint16_t sampleLength, const Arg::TypeID types[])
            : AbstractTelemetryChannel(fmt, wtr, argsAllocatorCapacity)
            , m_web(std::move(web))
            , m_receivedTypes(nullptr)
            , m_receivedSample(&m_sampleHeadBuffer[0])
            , m_sampleLength(sampleLength)
            , m_channelEnd(end)
        {
            initializeAfterConstruction(sampleLength, types);
        }
        bool readFull(void * data, uint16_t size, uint16_t tryCount = 8)
        {
            assert(m_channelEnd == EChannelEnd::Receiver);
            register auto p = static_cast<uint8_t*>(data);
            register auto end = p + size;
            register auto cnt = tryCount;
            while (p < end)
            {
                auto n = m_web.read(p, end - p);
                if (n) {
                    cnt = tryCount;
                } else {
                    if (--cnt == 0) {
                        return false;
                    }
                }
                p += n;
            }
            return true;
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
            , m_receivedTypes(rhs.m_receivedTypes)
            , m_receivedSample(rhs.m_receivedSample)
            , m_sampleLength(rhs.m_sampleLength)
            , m_channelEnd(rhs.m_channelEnd)
        {
            if (rhs.m_receivedSample == &rhs.m_sampleHeadBuffer[0]) {
                m_receivedSample = &this->m_sampleHeadBuffer[0];
            }
            rhs.m_receivedTypes = nullptr;
            rhs.m_receivedSample = nullptr;
            rhs.m_sampleLength = 0;
        }
        ~WebTelemetryChannel()
        {
            delete[] m_receivedTypes;
            if (&m_sampleHeadBuffer[0] != m_receivedSample) delete[] m_receivedSample;
        }
        /** Constructs the receiver end of the channel */
        static WebTelemetryChannel createReceiver(uint16_t port, const uint8_t (&ipAddr)[4], AbstractTelemetryFormatter & fmt, AbstractWriter & wtr)
        {
            UDPSocket sock{EAddressFamily::IPv4};
            if (sock.bind(IPAddress{port,ipAddr})) {
                WebTelemetryChannel ret{
                    EChannelEnd::Receiver,
                    std::move(sock),
                    fmt, wtr,
                    0,
                    0, nullptr
                };
                ret.m_sampleLength = 1;
                ret.m_receivedSample = &ret.m_sampleHeadBuffer[0];
                return std::move(ret);
            }
        }
        /** Constructs the sender end of the channel */
        static WebTelemetryChannel createSender(uint16_t port, const uint8_t (&ipAddr)[4], uint32_t argsAllocatorCapacity, uint16_t sampleLength, const Arg::TypeID types[])
        {
            UDPSocket sock{EAddressFamily::IPv4};
            if (sock.connect(IPAddress{port, ipAddr})) {
                return WebTelemetryChannel{
                    EChannelEnd::Sender,
                    std::move(sock),
                    m_fmt, m_wtr,
                    argsAllocatorCapacity,
                    sampleLength, types
                };
            } else {
                throw std::runtime_error{ "Failed to create WebTelemetryChannel sender" };
            }
        }
        virtual uint16_t sampleLength() const final override
        {
            return m_sampleLength;
        }
        virtual bool tryProcessSample() final override
        {
            assert(m_channelEnd == EChannelEnd::Receiver);
            assert(m_sampleLength > 0);

            bool ok = readFull(&m_receivedSample[0], sizeof(Arg) * m_sampleLength);
            if (!ok) {
                return false;
            }

            if (m_receivedSample[0].type == Arg::TypeID::__TYPE_ID_COUNT && m_receivedSample[0].valueOrArray.ArrayPointer == (void*) 0xFAFAFAFA)
            {
                m_sampleLength = m_receivedSample[0].arrayLength;
                m_receivedSample = new Arg[m_sampleLength];
                m_receivedTypes = new Arg::TypeID[m_sampleLength];
                readFull(&m_receivedTypes[0], sizeof(Arg::TypeID) * m_sampleLength);

                this->AbstractTelemetryChannel::m_formatter.formatExpectedTypes(this->AbstractTelemetryChannel::m_sink, m_sampleLength, m_receivedTypes);

                auto succ = tryProcessSample();
                return succ;
            }
            else
            {
                for (uint16_t i = 0; i < m_sampleLength; ++i)
                {
                    register Arg & arg = m_receivedSample[i];
                    if (arg.type & Arg::TypeID::__ISARRAY)
                    {
                        auto size = Arg::typeSize(arg.type) * arg.arrayLength;
                        readFull(const_cast<void*&>(arg.valueOrArray.ArrayPointer) = new char[size], size);
                    }
                }
                this->AbstractTelemetryChannel::m_formatter.formatValues(this->AbstractTelemetryChannel::m_sink, m_receivedSample);
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

TEST_CASE("UDPSocket", "[web][ip][socket][benchmark]")
{
    namespace web = _utl::logging;

    try {
        web::UDPSocket receiver{web::EAddressFamily::IPv4};
        web::UDPSocket sender{web::EAddressFamily::IPv4};

        bool bound = receiver.bind(web::IPAddress{12, {127,0,0,1}});
        bool connected = bound && sender.connect(web::IPAddress{12, {127,0,0,1}});

        char sentMsg[] = "1234567890";
        char recvMsg[20];

        sender.write(sentMsg, 10);
        auto len = receiver.read(recvMsg, 10);

        REQUIRE(len == 10);
        REQUIRE(std::memcmp(sentMsg, recvMsg, 10) == 0);
    }
    catch (std::exception & exc) {
        std::cout << exc.what();
        std::cout << std::endl;
    }
}
/*
TEST_CASE("WebTelemetryChannel smoke test", "[web][telemetry][channel]")
{
    using namespace _utl::logging;
    std::packaged_task<void()> producer{
        []() {
            try {
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
            catch (...) {
                std::cout << "THAT'S SOME BAD HAT, HARRY" << std::endl;
            }
        }
    };
    
    try {
        std::stringstream stream;
        PlainTextTelemetryFormatter fmt;
        StreamWriter wtr{stream};
        std::thread producerThread{ std::move(producer) };

        try {
            auto chanRecv = WebTelemetryChannel::createReceiver(12, {127,0,0,1}, fmt, wtr);
            CHECK(chanRecv.tryProcessSample());
            CHECK(chanRecv.tryProcessSample());
            CHECK(chanRecv.tryProcessSample());
            CHECK(chanRecv.tryProcessSample());
        }
        catch (std::exception & exc) {
            std::cout << exc.what() << std::endl;
        }
        if (producerThread.joinable()) producerThread.join();

        std::cout << stream.str() << std::endl;
    }
    catch (std::exception & exc) {
        std::cout << exc.what() << std::endl;
    }
}
*/
#endif
