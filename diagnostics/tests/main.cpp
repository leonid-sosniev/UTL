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
#include <utl/diagnostics/logging/formatters/PlainTextFormatters.hpp>
#include <utl/diagnostics/logging/channels/InterThreadChannels.hpp>
#include <utl/diagnostics/logging/channels/WebChannels.hpp>

#include <inttypes.h>

using namespace _utl::logging;
using namespace _utl;
namespace ch = std::chrono;

const char * strArr[2] = { "4", "some text" };

namespace {
    class CancellationToken {
        std::atomic_bool & m_flag;
    public:
        CancellationToken(std::atomic_bool & flag) : m_flag(flag)
        {}
        bool isCancelled() const {
            if (m_flag) {
                return true;
            } else {
                return false;
            }
        }
    };

    class ThreadWorker {
    private:
        std::atomic_bool writerIsCancelled;
        std::packaged_task<void(CancellationToken)> task;
        std::thread thread;
    public:
        ThreadWorker(std::function<void(CancellationToken)> procedure)
            : task(procedure)
            , thread(std::move(task), CancellationToken{writerIsCancelled})
            , writerIsCancelled(false)
        {}
        ~ThreadWorker() {
            writerIsCancelled = true;
            thread.join();
        }
    };

    template<typename InputDataType> class ThreadPool {
        struct WorkItem {
            InputDataType input;
            void(*func)(CancellationToken, InputDataType &);
        };
        using Worker = std::thread;
        internal::ConcurrentQueue<WorkItem> m_workItems;
        std::condition_variable m_workersCVar;
        std::mutex m_workersMutex;
        std::vector<Worker> m_workers;
        std::atomic<bool> m_stopAll;
    public:
        ~ThreadPool()
        {
            m_stopAll.store(true);
            m_workersCVar.notify_all();
            for (Worker & w : m_workers) w.join();
        }
        ThreadPool(uint32_t threadCount, uint32_t queueLength) : m_workItems(queueLength)
        {
            m_workers.reserve(threadCount);
            for (uint32_t i = 0; i < threadCount; ++i) {
                m_workers.emplace_back(&ThreadPool::threadMain, this);
            }
        }
        void enqueueTask(
                void(*func)(CancellationToken, const InputDataType &),
                InputDataType && input)
        {
            auto thereIsWorkPending = !m_workItems.isEmpty();
            while (m_workItems.tryEnqueue(WorkItem{ std::move(input), func }) == false) {
                m_workersCVar.notify_one();
            }
            if (thereIsWorkPending) {
                m_workersCVar.notify_one();
            }
        }
        bool workIsPending() const {
            return !m_workItems.isEmpty();
        }
    private:
        void threadMain()
        {
            std::cout << "ThreadPool worker #" << std::this_thread::get_id() << std::endl;
            CancellationToken token{m_stopAll};
            WorkItem workItem;
            std::function<bool()> wakeupCondition = [&]() {
                if (token.isCancelled()) {
                    return true;
                } else {
                    if (m_workItems.tryDequeue(workItem)) {
                        return true;
                    } else {
                        return false;
                    }
                }
            };

            for (;;) {
                {
                    std::unique_lock<std::mutex> lock{m_workersMutex};
                    m_workersCVar.wait(lock, wakeupCondition);
                }
                if (token.isCancelled()) {
                    return;
                } else {
                    workItem.func(token, workItem.input);
                }
            }
        }
    };
}
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

TEST_CASE("smoke sequential", "[interthread][event][channel][validation]")
{
    static std::array<char,1024> buf;
    std::fill(buf.begin(), buf.end(), '\xDB');

    RawEventFormatter fmt{};
    FlatBufferWriter wtr{ buf.data(), buf.size() };
    InterThreadEventChannel chan{ fmt, wtr, 64, 300 };

    // all data fits in the buffer
    auto p_1 = (char*) wtr.position();
    for (int i = 0; i < 2; ++i) { UTL_logev(chan, "1234567890-", 1u, -1, 0.2, '3', strArr[i]); }
    for (int i = 0; i < 2; ++i) { while (!chan.tryReceiveAndProcessEvent()) {} }
    VALIDATE_INTERTHREAD_OUTPUT(p_1);

    // wrapping up the buffer end
    auto p_2 = (char*) wtr.position();
    for (int i = 0; i < 2; ++i) { UTL_logev(chan, "1234567890-", 1u, -1, 0.2, '3', strArr[i]); }
    for (int i = 0; i < 2; ++i) { while (!chan.tryReceiveAndProcessEvent()) {} }
    VALIDATE_INTERTHREAD_OUTPUT(p_2);
}

TEST_CASE("local inter-threaded", "[interthread][event][channel][validation]")
{
    static std::array<char,1024> buf;
    std::fill(buf.begin(), buf.end(), '\xDB');

    RawEventFormatter fmt{};
    FlatBufferWriter wtr{ buf.data(), buf.size() };
    InterThreadEventChannel chan{ fmt, wtr, 16, 128 }; // -- too small to store two events -- only one at a time
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
        [&chan]() {
            try {
                for (int i = 0; i < 2; ++i) {
                    UTL_logev(chan, "1234567890-", 1u, -1, 0.2, '3', strArr[i % 2]);
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

TEST_CASE("socket-based (inter-threaded)", "[web][event][channel][validation]")
{
    std::packaged_task<void()> sender{
        []() {
            std::this_thread::sleep_for(std::chrono::seconds{10});
            auto wchan = std::move(WebEventChannel::createSender(1024, 12345, { 127, 0, 0, 1 }));
            for (int i = 0; i < 2; ++i) {
                UTL_logev(*wchan, "1234567890-", 1u, -1, 0.2, '3', strArr[i]);
            }
        }
    };
    std::packaged_task<void()> receiver{
        []() { try {
            std::array<char,1024> buf;
            std::fill(buf.begin(), buf.end(), '\xDB');
            FlatBufferWriter wtr{ buf.data(), buf.size() };
            RawEventFormatter fmt{};
            auto rchan = std::move(WebEventChannel::createReceiver(fmt, wtr, 12345, { 127, 0, 0, 1 }));
            for (int i = 0; i < 2; ++i) {
                while (!rchan->tryReceiveAndProcessEvent()) {}
            }
        } catch (std::exception &exc) { std::cout << exc.what() << std::endl; }}
    };
    std::thread th2{ std::move(receiver) };
    std::thread th1{ std::move(sender) };
    th1.join();
    th2.join();
}

inline uint64_t rdtsc()
{
#if defined(GCC)
    uint32_t lo, hi;
    asm volatile ( "rdtsc\n" : "=a" (lo), "=d" (hi) );
#elif defined(__clang__)
    return __rdtsc();
#else
    #error "The platform is not supported"
#endif
}

TEST_CASE("InterThreadEventChannel benchmark", "[interthread][event][channel][benchmark]")
{
    unsigned bufferSize = 0;
    const char * name;
    DummyWriter wtr{};
    PlainTextEventFormatter fmt;

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
    InterThreadEventChannel chan{ fmt, wtr, 1024, bufferSize };
    ThreadWorker C{
        [&chan](CancellationToken token) {
            try {
                while (chan.tryReceiveAndProcessEvent() || !token.isCancelled()) {}
            }
            catch (std::exception & exc) {
                std::cout << exc.what() << std::endl;
            }
            catch (...) {}
        }
    };
    BENCHMARK(name) {
        UTL_logev(chan, "this is some message to be logged here and consumed", -1, 'w', "some words", 0.56);
    };
}

TEST_CASE("WebTelemetryChannel benchmark", "[web][telemetry][channel][benchmark]")
{
    ThreadWorker C{
        [](CancellationToken token) {
            PlainTextTelemetryFormatter fmt;
            DummyWriter wtr{};
            auto chan = WebTelemetryChannel::createReceiver(54321, {127,0,0,1}, fmt, wtr);
            try {
                while (chan.tryProcessSample() || !token.isCancelled()) {}
            }
            catch (std::exception & exc) {
                std::cout << exc.what() << std::endl;
            }
            catch (...) {}
        }
    };
    Arg::TypeID types[4] = {
        Arg::TypeID::TI_i32, Arg::TypeID::TI_Char, Arg::TypeID::TI_arrayof_Char, Arg::TypeID::TI_f64
    };
    auto chan = WebTelemetryChannel::createSender(54321, {127,0,0,1}, 1024, 4, types);
    BENCHMARK("") {
        UTL_logsam(chan, -1, 'w', "some words", 0.56);
    };
}

inline std::chrono::nanoseconds runEmptyLoop(int iterations)
{
    auto t_0 = std::chrono::system_clock::now();
    for (int i = 0; i < iterations; ++i) {}
    return std::chrono::system_clock::now() - t_0;
}

TEST_CASE("InterThreadTelemetryChannel benchmark", "[interthread][telemetry][channel][benchmark]")
{
    DummyWriter wtr{};
    PlainTextTelemetryFormatter fmt;
    std::unique_ptr<InterThreadTelemetryChannel> chan;
    std::function<void(CancellationToken)> consumer = [&chan](CancellationToken token) {
        try {
            while (chan->tryProcessSample() || !token.isCancelled()) {}
        }
        catch (std::exception & exc) {
            std::cout << exc.what() << std::endl;
        }
        catch (...) {}
    };


#define BENCHMARK_RUN(BUFFER_SIZE, NAME_STR) \
    SECTION("InterThreadTelemetryChannel " NAME_STR) { \
        static const Arg::TypeID types[] = { \
            Arg::TypeID::TI_arrayof_Char, Arg::TypeID::TI_i32, Arg::TypeID::TI_Char, \
            Arg::TypeID::TI_arrayof_Char, Arg::TypeID::TI_f32, Arg::TypeID::TI_EpochNsec \
        }; \
        chan.reset(new InterThreadTelemetryChannel{fmt, wtr, 1024, BUFFER_SIZE, 6, types}); \
        ThreadWorker C{consumer}; \
        BENCHMARK("InterThreadTelemetryChannel " NAME_STR) { \
            UTL_logsam(*chan, "this is some message to be logged here and consumed", -1, 'w', "some words", 0.56f, std::chrono::system_clock::now()); \
        }; \
        auto t_0 = std::chrono::system_clock::now(); \
        for (int i = 0; i < 1'000'000; ++i) { \
            UTL_logsam(*chan, "this is some message to be logged here and consumed", -1, 'w', "some words", 0.56f, std::chrono::system_clock::now()); \
        } \
        std::cerr << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - t_0 - runEmptyLoop(1'000'000)).count() \
        << " nsec for 1M samples" << std::endl; \
    }

    BENCHMARK_RUN(1024, "1kB");
    BENCHMARK_RUN(16384, "16kB");
    BENCHMARK_RUN(65536, "64kB");
    BENCHMARK_RUN(1024*1024, "1MB");
    BENCHMARK_RUN(16*1024*1024, "16MB");

#undef BENCHMARK_RUN
}