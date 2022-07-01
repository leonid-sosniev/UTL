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

#include <inttypes.h>

using namespace _utl::logging;
using namespace _utl;
namespace ch = std::chrono;

const char * strArr[2] = { "4", "some text" };

namespace {
    static constexpr char hundred[100][2] = {
        { '0','0' }, { '0','1' }, { '0','2' }, { '0','3' }, { '0','4' }, { '0','5' }, { '0','6' }, { '0','7' }, { '0','8' }, { '0','9' },
        { '1','0' }, { '1','1' }, { '1','2' }, { '1','3' }, { '1','4' }, { '1','5' }, { '1','6' }, { '1','7' }, { '1','8' }, { '1','9' },
        { '2','0' }, { '2','1' }, { '2','2' }, { '2','3' }, { '2','4' }, { '2','5' }, { '2','6' }, { '2','7' }, { '2','8' }, { '2','9' },
        { '3','0' }, { '3','1' }, { '3','2' }, { '3','3' }, { '3','4' }, { '3','5' }, { '3','6' }, { '3','7' }, { '3','8' }, { '3','9' },
        { '4','0' }, { '4','1' }, { '4','2' }, { '4','3' }, { '4','4' }, { '4','5' }, { '4','6' }, { '4','7' }, { '4','8' }, { '4','9' },
        { '5','0' }, { '5','1' }, { '5','2' }, { '5','3' }, { '5','4' }, { '5','5' }, { '5','6' }, { '5','7' }, { '5','8' }, { '5','9' },
        { '6','0' }, { '6','1' }, { '6','2' }, { '6','3' }, { '6','4' }, { '6','5' }, { '6','6' }, { '6','7' }, { '6','8' }, { '6','9' },
        { '7','0' }, { '7','1' }, { '7','2' }, { '7','3' }, { '7','4' }, { '7','5' }, { '7','6' }, { '7','7' }, { '7','8' }, { '7','9' },
        { '8','0' }, { '8','1' }, { '8','2' }, { '8','3' }, { '8','4' }, { '8','5' }, { '8','6' }, { '8','7' }, { '8','8' }, { '8','9' },
        { '9','0' }, { '9','1' }, { '9','2' }, { '9','3' }, { '9','4' }, { '9','5' }, { '9','6' }, { '9','7' }, { '9','8' }, { '9','9' },
    };
    static constexpr uint8_t decimalDigitsNumberByVarSize[1 + 8] = {
    //  0  1    2    3  4   5  6  7  8
        0, 3+1, 5+1, 0, 10, 0, 0, 0, 20 // digit count must be even 'cause printDecimal prints digit pairs
    };

    inline auto printDecimal(char * buffer, const float & number, char *& out_end) -> char*
    {
        out_end = buffer + std::snprintf(buffer, 32, "%0.7f", number);
        return buffer;
    }
    inline auto printDecimal(char * buffer, const double & number, char *& out_end) -> char*
    {
        out_end = buffer + std::snprintf(buffer, 32, "%0.16f", number);
        return buffer;
    }
    template<typename T>
    inline auto printDecimal(char * buffer, const T & number, char *& out_end)
        -> typename std::enable_if<std::is_unsigned<T>::value && std::is_integral<T>::value,char*>::type
    {
        using N = typename std::remove_cv<T>::type;
        N num = number;
        register char *p = buffer + decimalDigitsNumberByVarSize[sizeof(T)];
        out_end = p;
        do {
            const char (&pair)[2] = hundred[num % 100];
            p -= 2;
            *reinterpret_cast<uint16_t *>(p) = *reinterpret_cast<const uint16_t *>(pair);
            num /= 100;
        }
        while (num);
        register auto p2 = p + 1;
        return *p == '0' ? p2 : p;
    }
    template<typename T>
    inline auto printDecimal(char * buffer, const T & number, char *& out_end)
        -> typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value,char*>::type
    {
        using U = typename std::make_unsigned<T>::type;
        using N = typename std::remove_cv<T>::type;
        N num2 = -number;
        N num = number;
        char * p = printDecimal(buffer + 1, U(num >= 0 ? num : num2), out_end);
        if (num < 0) *--p = '-';
        return p;
    }

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
        ConcurrentQueue<WorkItem> m_workItems;
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

class PlainTextEventFormatter : public AbstractEventFormatter {
public:
    void formatEventAttributes(AbstractWriter &, const EventAttributes &) override {
        return;
    }
    void formatEvent(AbstractWriter & wtr, const EventAttributes & attr, const Arg arg[]) override {
        AbstractWriter * writer = &wtr;
        char * fmtStr;
        char * fmtEnd;
        char fmtBuf[64];

        writer->write("[ ", 2);
        writer->write(attr.function.str, attr.function.end - attr.function.str);
        writer->write(" - ", 3);
        writer->write(attr.file.str, attr.file.end - attr.file.str);
        writer->write(": ", 2);
        fmtStr = printDecimal<uint32_t>(fmtBuf, attr.line, fmtEnd);
        writer->write(fmtStr, fmtEnd - fmtStr);
        writer->write(" ]", 2);

        writer->write(" \"", 2);
        writer->write(attr.messageFormat.str, attr.messageFormat.end - attr.messageFormat.str);
        writer->write("\" ", 2);

        for (uint16_t i = 0; i < attr.argumentsExpected; ++i,++arg) {
            writer->write(" // ", 4);
            switch (arg->type) {
                #define WRITE_ARRAY_OF(TYPE) \
                    for (uint32_t i = 0; i < arg->arrayLength; ++i) { \
                        fmtStr = printDecimal(fmtBuf, static_cast<const TYPE*>(arg->valueOrArray.ArrayPointer)[i], fmtEnd); \
                        *fmtEnd++ = ','; \
                        writer->write(fmtStr,fmtEnd-fmtStr); \
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
                case Arg::TypeID::TI_arrayof_Char:      writer->write((char*) arg->valueOrArray.ArrayPointer, arg->arrayLength); writer->write(",",1); break;
                case Arg::TypeID::TI_arrayof_Thread:    break;
                case Arg::TypeID::TI_arrayof_EpochNsec: break;
                #undef WRITE_ARRAY_OF
                case Arg::TypeID::TI_u8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_u16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_u32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_u64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.u64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_i8:        fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i8 , fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_i16:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i16, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_i32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_i64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.i64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_f32:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f32, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
                case Arg::TypeID::TI_f64:       fmtStr = printDecimal(fmtBuf, arg->valueOrArray.f64, fmtEnd); *fmtEnd++ = ','; writer->write(fmtStr,fmtEnd-fmtStr);  break;
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

TEST_CASE("local inter-threaded", "[validation]")
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

TEST_CASE("socket-based (inter-threaded)", "[validation]")
{
    std::packaged_task<void()> sender{
        []() {
            std::this_thread::sleep_for(std::chrono::seconds{10});
            auto wchan = WebEventChannel::createSender(1024, 12345, { 127, 0, 0, 1 });
            for (int i = 0; i < 2; ++i) {
                UTL_logev(*wchan, "1234567890-", 1u, -1, 0.2, '3', strArr[i]);
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

TEST_CASE("InterThreadEventChannel benchmark", "[benchmark]")
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
        UTL_logev(chan, "this is some message to be logged here and consumed");
    };
}