//#define CATCH_CONFIG_MAIN
//#define CATCH_CONFIG_ENABLE_BENCHMARKING
//#include <utl/Catch2/single_include/catch2/catch.hpp>
#include <utl/tester.hpp>

#include <cassert>

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
#include <vector>
#include <utl/io/Writer.hpp>
#include <utl/diagnostics/logging.hpp>
#include <utl/diagnostics/logging/writers.hpp>
//#include <utl/diagnostics/logging/formatters/PlainTextFormatters.hpp>
//#include <utl/diagnostics/logging/channels/InterThreadChannels.hpp>
//#include <utl/diagnostics/logging/channels/WebChannels.hpp>

#include <inttypes.h>

using namespace _utl::logging;
using namespace _utl;
namespace ch = std::chrono;

const char * strArr[2] = { "4", "some text" };

/*
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
        internal::ManyReadersManyWritersQueue<WorkItem> m_workItems;
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
*/

class DummyEventFormatter
    : public AbstractEventFormatter
{
private:
    void formatEventAttributes_(MemoryResource & mem, const EventAttributes & attr) override {}
    void formatEvent_(MemoryResource & mem, const EventAttributes & attr, const Arg args[]) override {}
};

class RawEventFormatter
    : public AbstractEventFormatter
{
private:
    void formatEventAttributes_(MemoryResource & mem, const EventAttributes & attr) override;
    void formatEvent_(MemoryResource & mem, const EventAttributes & attr, const Arg args[]) override;
};

void RawEventFormatter::formatEventAttributes_(MemoryResource & mem, const EventAttributes & attr)
{
    assert(&attr.id                < (void*) &attr.line             );
    assert(&attr.line              < (void*) &attr.argumentsExpected);
    mem.submitStaticConstantData("!!!!", 4);
    mem.submitStaticConstantData(&attr.id, sizeof(attr.id) + sizeof(attr.line) + sizeof(attr.argumentsExpected));

    auto lenBuf = (uint64_t*) mem.allocate(sizeof(uint64_t) * 3);
    auto len0 = lenBuf[0] = attr.messageFormat.end - attr.messageFormat.str;
    auto len1 = lenBuf[1] = attr.function.end      - attr.function.str     ;
    auto len2 = lenBuf[2] = attr.file.end          - attr.file.str         ;
    mem.submitAllocated(sizeof(uint64_t) * 3);

    mem.submitStaticConstantData(attr.messageFormat.str , len0);
    mem.submitStaticConstantData(attr.function.str      , len1);
    mem.submitStaticConstantData(attr.file.str          , len2);
}
EventAttributes parseEventAttributes(const char * buff, const char ** out_cursor)
{
    auto start = buff;
    EventAttributes attr;
    assert(std::memcmp(buff, "!!!!", 4) == 0);                                                          buff += 4;
    std::memcpy(&attr.id, buff, sizeof(attr.id) + sizeof(attr.line) + sizeof(attr.argumentsExpected));  buff += sizeof(attr.id) + sizeof(attr.line) + sizeof(attr.argumentsExpected);

    uint64_t len[3]; std::memcpy(&len[0], buff, sizeof(uint64_t) * 3);                                  buff += sizeof(uint64_t) * 3;

    char *ptr0 = new char[len[0] + 1];  std::memcpy(ptr0, buff, len[0]);                                buff += len[0];
    char *ptr1 = new char[len[1] + 1];  std::memcpy(ptr1, buff, len[1]);                                buff += len[1];
    char *ptr2 = new char[len[2] + 1];  std::memcpy(ptr2, buff, len[2]);                                buff += len[2];
    attr.messageFormat = Str{ ptr0, ptr0+len[0] };  ptr0[len[0]] = '\0';    
    attr.function      = Str{ ptr1, ptr1+len[1] };  ptr1[len[1]] = '\0';    
    attr.file          = Str{ ptr2, ptr2+len[2] };  ptr2[len[2]] = '\0';    

    if (out_cursor) {
        *out_cursor = buff;
    }
    return attr;
}

void RawEventFormatter::formatEvent_(MemoryResource & mem, const EventAttributes & attr, const Arg args[])
{
    mem.submitStaticConstantData("::::", 4);
    mem.submitStaticConstantData(&attr.id, sizeof(attr.id));
    mem.submitStaticConstantData(&attr.argumentsExpected, sizeof(attr.argumentsExpected));

    auto argsEnd = args + attr.argumentsExpected;
    auto argsSize = sizeof(Arg) * attr.argumentsExpected;
    auto alloc = (Arg*) mem.allocate(argsSize);
    std::copy(args, argsEnd, alloc);
    mem.submitAllocated(argsSize);

    for (auto arg = args; arg < argsEnd; ++arg)
    {
        if (int(arg->type) & (int) Arg::TypeID::__ISARRAY)
        {
            auto sz = Arg::typeSize(arg->type) * arg->arrayLength;
            auto buf = mem.allocate(sz);
            std::memcpy(buf, arg->valueOrArray.ArrayPointer, sz);
            mem.submitAllocated(sz);
        }
    }
}
std::unique_ptr<Arg[]> parseEvent(const char * buff, const char ** out_cursor)
{
    auto start = buff;
    size_t attr_argumentsExpected;
    EventID attr_id;

    assert(std::memcmp(buff, "::::", 4) == 0);                                      buff += 4;
    std::memcpy(&attr_id, buff, sizeof(attr_id));                                   buff += sizeof(attr_id);
    std::memcpy(&attr_argumentsExpected, buff, sizeof(attr_argumentsExpected));     buff += sizeof(attr_argumentsExpected);

    auto result = std::make_unique<Arg[]>(attr_argumentsExpected);
    Arg *args = result.get();
    Arg *argsEnd = args + attr_argumentsExpected;
    std::memcpy(args, buff, sizeof(Arg) * attr_argumentsExpected);                  buff += sizeof(Arg) * attr_argumentsExpected;

    for (Arg *arg = args; arg < argsEnd; ++arg)
    {
        if (int(arg->type) & (int) Arg::TypeID::__ISARRAY)
        {
            auto sz = Arg::typeSize(arg->type) * arg->arrayLength;
            auto p = new char[sz];
            std::memcpy(p, buff, sz);                                               buff += sz;
            arg->valueOrArray.ArrayPointer = p;
        }
    }
    if (out_cursor) {
        *out_cursor = buff;
    }
    return std::move(result);
}

#define REQ(T,value) \
    REQUIRE(*(T*)&buf[ofs] == (T)value);\
    ofs += sizeof(T);

void printDump(const char * data, size_t size)
{
    std::cerr << "==================================================================================================" << std::endl;
    for (size_t i = 0; i < size; ++i)
    {
        std::cerr << ' ';
        char C = data[i];
        if (std::isprint(C) || std::isblank(C)) {
            std::cerr << ' ' << C;
        } else {
            std::cerr << std::hex << std::setw(2) << std::setfill('0') << (uint16_t) (uint8_t) C;
        }
    }
    std::cerr << std::endl;
}



//TEST_CASE("smoke sequential", "[interthread][event][channel][validation]")
int main()
{
    static std::array<char,512> buf;
    std::fill(buf.begin(), buf.end(), '\xDB');

    //RawEventFormatter fmt{};
    DummyEventFormatter fmt{};
    FlatBufferWriter wtr{ buf.data(), buf.size() };
/*
    {
    Logger chan(fmt, wtr, 64, 300, 1024, 300, "Logger");

    // all data fits in the buffer
    for (int i = 0; i < 2; ++i)
    {
        UTL_logev(chan, "1234567890-", 1u, -1-i, 0.2, '3', strArr[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds{30});
        printDump(buf.data(), buf.size());
        std::this_thread::sleep_for(std::chrono::milliseconds{30});
    }
    
    const char * cursor = buf.data();
    auto attr = parseEventAttributes(cursor, &cursor);
    auto args1 = parseEvent(cursor, &cursor);
    auto args2 = parseEvent(cursor, &cursor);

    assert(args1[0].type == Arg::TypeID::TI_u32);            assert(args1[0].valueOrArray.u32                         ==   1);
    assert(args1[1].type == Arg::TypeID::TI_i32);            assert(args1[1].valueOrArray.i32                         ==  -1);
    assert(args1[2].type == Arg::TypeID::TI_f64);            assert(args1[2].valueOrArray.f64                         == 0.2);
    assert(args1[3].type == Arg::TypeID::TI_Char);           assert(args1[3].valueOrArray.Char                        == '3');
    assert(args1[4].type == Arg::TypeID::TI_arrayof_Char);   assert(args1[4].valueOrArray.debugPtr == std::string(strArr[0]));

    assert(args2[0].type == Arg::TypeID::TI_u32);            assert(args2[0].valueOrArray.u32                         ==   1);
    assert(args2[1].type == Arg::TypeID::TI_i32);            assert(args2[1].valueOrArray.i32                         ==  -2);
    assert(args2[2].type == Arg::TypeID::TI_f64);            assert(args2[2].valueOrArray.f64                         == 0.2);
    assert(args2[3].type == Arg::TypeID::TI_Char);           assert(args2[3].valueOrArray.Char                        == '3');
    assert(args2[4].type == Arg::TypeID::TI_arrayof_Char);   assert(args2[4].valueOrArray.debugPtr == std::string(strArr[1]));
    }
*/
    _utl::DummyWriter dwtr;

    Logger l{fmt, dwtr, 8'000, 8'000, 2, 2};//, "L"};

    auto t_0 = std::chrono::steady_clock::now();
    size_t N = 1'000'000;
    for (size_t i = 0; i < N; ++i)
    {
        UTL_logev(l, "1234567890-", 1u, -1-i, 0.2, '3', strArr[i%2]);
    }
    auto t_1 = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(t_1 - t_0);

    std::cout << "All events take " << dur.count() << "ns; 1 event takes " << (dur / N).count() << "ns" << std::endl;

    return 0;
    // // wrapping up the buffer end
    // auto p_2 = (char*) wtr.position();
    // for (int i = 0; i < 2; ++i) { UTL_logev(chan, "1234567890-", 1u, -1, 0.2, '3', strArr[i]); }
    // for (int i = 0; i < 2; ++i) { while (!chan.tryReceiveAndProcessEvent()) {} }
    // VALIDATE_INTERTHREAD_OUTPUT(p_2);
}

/*
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
    return 0;
    //#error "The platform is not supported"
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

*/