#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define DEBUG
#include <utl/Catch2/single_include/catch2/catch.hpp>

#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utl/io/Writer.hpp>
#include <utl/diagnostics/logging.hpp>
#include <utl/diagnostics/logging/writers.hpp>
#include <utl/diagnostics/logging/formatters.hpp>
#include <utl/diagnostics/logging/channels.hpp>

using namespace _utl::logging;
using namespace _utl;
namespace ch = std::chrono;

const char * strArr[2] = { "4", "some text" };

class PlainTextEventFormatter : public AbstractEventFormatter {
public:
    void formatEventAttributes(AbstractWriter &, const EventAttributes &) override {
        return;
    }
    void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg arg[]) override {
        AbstractWriter * writer = &wtr;

        writer->write("[ ", 2);
        writer->write(attr.function.str, attr.function.end - attr.function.str);
        writer->write(" - ", 3);
        writer->write(attr.file.str, attr.file.end - attr.file.str);
        writer->write(": ", 2);
        auto ln = std::to_string(attr.line); writer->write(ln.data(), ln.size());
        writer->write(" ]", 2);

        writer->write(" \"", 2);
        writer->write(attr.messageFormat.str, attr.messageFormat.end - attr.messageFormat.str);
        writer->write("\" ", 2);

        for (uint16_t i = 0; i < attr.argumentsExpected; ++i) {
            writer->write(" // ", 4);
            std::string x;
            switch (arg->type) {
                #define WRITE_ARRAY_OF(TYPE) \
                    for (uint32_t i = 0; i < arg->arrayLength; ++i) { \
                        x = std::to_string(static_cast<const TYPE*>(arg->valueOrArray.ArrayPointer)[i]); \
                        writer->write(x.data(),x.size()); \
                        writer->write(",",1); \
                    }
                case Arg::TypeID::TI_arrayof_u8:        WRITE_ARRAY_OF(uint8_t);  break;
                case Arg::TypeID::TI_arrayof_u16:       WRITE_ARRAY_OF(uint16_t); break;
                case Arg::TypeID::TI_arrayof_u32:       WRITE_ARRAY_OF(uint32_t); break;
                case Arg::TypeID::TI_arrayof_u64:       WRITE_ARRAY_OF(uint64_t); break;
                case Arg::TypeID::TI_arrayof_i8:        WRITE_ARRAY_OF(int8_t);   break;
                case Arg::TypeID::TI_arrayof_i16:       WRITE_ARRAY_OF(int16_t);  break;
                case Arg::TypeID::TI_arrayof_i32:       WRITE_ARRAY_OF(int32_t);  break;
                case Arg::TypeID::TI_arrayof_i64:       WRITE_ARRAY_OF(int64_t);  break;
                case Arg::TypeID::TI_arrayof_f32:       WRITE_ARRAY_OF(float);    break;
                case Arg::TypeID::TI_arrayof_f64:       WRITE_ARRAY_OF(double);   break;
                case Arg::TypeID::TI_arrayof_Char:      writer->write((char*) arg->valueOrArray.ArrayPointer, arg->arrayLength); break;
                case Arg::TypeID::TI_arrayof_Thread:    break;
                case Arg::TypeID::TI_arrayof_EpochNsec: break;
                #undef WRITE_ARRAY_OF
                case Arg::TypeID::TI_u8:        x = std::to_string(arg->valueOrArray.u8 );  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_u16:       x = std::to_string(arg->valueOrArray.u16);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_u32:       x = std::to_string(arg->valueOrArray.u32);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_u64:       x = std::to_string(arg->valueOrArray.u64);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_i8:        x = std::to_string(arg->valueOrArray.i8 );  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_i16:       x = std::to_string(arg->valueOrArray.i16);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_i32:       x = std::to_string(arg->valueOrArray.i32);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_i64:       x = std::to_string(arg->valueOrArray.i64);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_f32:       x = std::to_string(arg->valueOrArray.f32);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_f64:       x = std::to_string(arg->valueOrArray.f64);  writer->write(x.data(),x.size());  break;
                case Arg::TypeID::TI_Char:      writer->write(&arg->valueOrArray.Char,1); break;
                case Arg::TypeID::TI_Thread:    break;
                case Arg::TypeID::TI_EpochNsec: break;
                default: break;
            }
        }
        writer->write("\n", 1);
    }
};
class RawEventFormatter : public AbstractEventFormatter {
public:
    void formatEventAttributes(AbstractWriter & wtr, const EventAttributes & attr) override {
        auto p = &attr;
        wtr.write("!", 1);
        wtr.write(&p, sizeof(p));
    }
    void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg args[]) override {
        auto p = &attr;
        wtr.write(":", 1);
        wtr.write(&p, sizeof(p));
        //
        auto end = args + attr.argumentsExpected;
        for (auto arg = args; arg < end; ++arg)
        {
            wtr.write(&arg->type, sizeof(Arg::TypeID));
            if (int(arg->type) & (int) Arg::TypeID::__ISARRAY) {
                wtr.write(&arg->arrayLength, sizeof(uint32_t));
                wtr.write(arg->valueOrArray.ArrayPointer, Arg::typeSize(arg->type) * arg->arrayLength);
            } else {
                wtr.write(&arg->valueOrArray, Arg::typeSize(arg->type));
            }
        }
    }
};

#define REQ(T,value) \
    REQUIRE(*(T*)&buf[ofs] == (T)value);\
    ofs += sizeof(T);
void VALIDATE_INTERTHREAD_OUTPUT(char * buf) {                           ;
    int ofs = 0;                                                         ;
    REQ(char, '!');                                                      ;
    auto* lp = *(EventAttributes**) &buf[ofs]; ofs += sizeof(void*); ;
    int i;                                                               ;
    for (int i = 0; i < 2; ++i) {                                        ;
        REQ(char, ':');                                                  ;
        REQ(void*, lp);                                                  ;
        REQ(Arg::TypeID, Arg::TypeID::TI_u32);                           ;
        REQ(uint32_t, 1);                                                ;
        REQ(Arg::TypeID, Arg::TypeID::TI_i32);                           ;
        REQ(int32_t, -1);                                                ;
        REQ(Arg::TypeID, Arg::TypeID::TI_f64);                           ;
        REQ(double, 0.2);                                                ;
        REQ(Arg::TypeID, Arg::TypeID::TI_Char);                          ;
        REQ(char, '3');                                                  ;
        REQ(Arg::TypeID, Arg::TypeID::TI_arrayof_Char);                  ;
        auto str = strArr[i];                                            ;
        uint32_t len = std::strlen(str);                                 ;
        REQ(uint32_t, len);                                              ;
        REQUIRE(std::memcmp(&buf[ofs],str,len) == 0); ofs += 1;          ;
    }                                                                    ;
}

TEST_CASE("smoke sequential", "[validation]")
{
    static std::array<char,1024> buf;
    std::fill(buf.begin(), buf.end(), '\xDB');

    RawEventFormatter fmt{};
    FlatBufferWriter wtr{ buf.data(), buf.size() };
    InterThreadEventChannel chan{ fmt, wtr, 300 };
    EventLogger<InterThreadEventChannel> logger{ chan }; 

    // all data fits in the buffer
    auto p_1 = (char*) wtr.position();
    for (int i = 0; i < 2; ++i) { UTL_logev(logger, "1234567890-", 1u, -1, 0.2, '3', strArr[i]); }
    for (int i = 0; i < 2; ++i) { while (!chan.tryReceiveAndProcessEvent()) {} }
    VALIDATE_INTERTHREAD_OUTPUT(p_1);

    // wrapping up the buffer end
    auto p_2 = (char*) wtr.position();
    for (int i = 0; i < 2; ++i) { UTL_logev(logger, "1234567890-", 1u, -1, 0.2, '3', strArr[i]); }
    for (int i = 0; i < 2; ++i) { while (!chan.tryReceiveAndProcessEvent()) {} }
    VALIDATE_INTERTHREAD_OUTPUT(p_2);
}

TEST_CASE("local inter-threaded", "[validation]")
{
    static std::array<char,1024> buf;
    std::fill(buf.begin(), buf.end(), '\xDB');

    RawEventFormatter fmt{};
    FlatBufferWriter wtr{ buf.data(), buf.size() };
    InterThreadEventChannel chan{ fmt, wtr, 128 }; // -- too small to store two events -- only one at a time
    EventLogger<InterThreadEventChannel> logger{ chan };

    std::packaged_task<void()> reader{
        [&wtr,&chan]() {
            try {
                auto p_1 = (char*) wtr.position();
                for (int i = 0; i < 2; ++i) {
                    while (!chan.tryReceiveAndProcessEvent()) {}
                }
                VALIDATE_INTERTHREAD_OUTPUT(p_1);
            } catch (std::exception & exc) {
                std::cerr << exc.what() << std::endl;
                REQUIRE(false);
            }
        }
    };
    std::packaged_task<void()> writer{
        [&logger]() {
            try {
                for (int i = 0; i < 2; ++i) {
                    UTL_logev(logger, "1234567890-", 1u, -1, 0.2, '3', strArr[i % 2]);
                }
            } catch (...) {
                REQUIRE(false);
            }
        }
    };

    std::thread A{ std::move(reader) };
    std::thread B{ std::move(writer) };
    A.join();
    B.join();
}

TEST_CASE("socket-based (inter-threaded)", "[validation]")
{
    std::packaged_task<void()> sender{
        []() {
            std::this_thread::sleep_for(std::chrono::seconds{10});
            auto wchan = WebEventChannel::createSender(12345, { 127, 0, 0, 1 });
            EventLogger<WebEventChannel> logger{ *wchan };
            for (int i = 0; i < 2; ++i) {
                UTL_logev(logger, "1234567890-", 1u, -1, 0.2, '3', strArr[i]);
            }
        }
    };
    std::packaged_task<void()> receiver{
        []() {
            std::array<char,1024> buf;
            std::fill(buf.begin(), buf.end(), '\xDB');
            FlatBufferWriter wtr{ buf.data(), buf.size() };
            RawEventFormatter fmt{};
            auto rchan = WebEventChannel::createReceiver(fmt, wtr, 12345, { 127, 0, 0, 1 });
            for (int i = 0; i < 2; ++i) {
                while (!rchan->tryReceiveAndProcessEvent()) {}
            }
        }
    };
    std::thread th2{ std::move(receiver) };
    std::thread th1{ std::move(sender) };
    th1.join();
    th2.join();
}

template<typename TChannel> class ThreadWorker {
    std::atomic_bool writerIsRunning;
    std::packaged_task<void()> task;
    std::thread thread;
public:
    ThreadWorker(TChannel & chan)
        : task([&]() {
            try {
                while (chan.tryReceiveAndProcessEvent() || writerIsRunning) {}
            }
            catch (std::exception & exc) {
                std::cout << exc.what() << std::endl;
            }
            catch (...) {}
        })
        , thread(std::move(task))
        , writerIsRunning(true)
    {}
    ~ThreadWorker() {
        writerIsRunning = false;
        thread.join();
    }
};

TEST_CASE("InterThreadEventChannel benchmark", "[benchmark]")
{
    unsigned bufferSize = 0;
    const char * name;
    DummyWriter wtr{};
    DummyEventFormatter fmt{};

    SECTION("InterThreadEventChannel 1kB" ) {
        bufferSize = 1024;
        name = "InterThreadEventChannel 1kB" ;
    }
    SECTION("InterThreadEventChannel 16kB") {
        bufferSize = 16384;
        name = "InterThreadEventChannel 16kB";
    }
    SECTION("InterThreadEventChannel 64kB") {
        bufferSize = 65536;
        name = "InterThreadEventChannel 64kB";
    }
    SECTION("InterThreadEventChannel 1MB" ) {
        bufferSize = 1024*1024;
        name = "InterThreadEventChannel 1MB" ;
    }
    SECTION("InterThreadEventChannel 16MB") {
        bufferSize = 16*1024*1024;
        name = "InterThreadEventChannel 16MB";
    }
    InterThreadEventChannel chan{ fmt, wtr, bufferSize };
    EventLogger<InterThreadEventChannel> log{ chan };
    ThreadWorker<InterThreadEventChannel> C{chan};
    BENCHMARK(name) {
        UTL_logev(log, "this is some message to be logged here and consumed");
    };
}