#pragma once

#include <utl/io/Writer.hpp>
#include <utl/threading/Spinlock.hpp>
#include <stdio.h>
#include <ctime>
#include <limits>
#include <thread>

namespace _utl
{
    namespace detail
    {
        namespace rolling_file_writer
        {
            struct BackgroundWriter {
            private:
                #define LOCK(x) std::unique_lock<decltype(x)> lock(x)
                struct Buffer
                {
                    char * data;
                    char * end;
                    char * cursor;
                    Buffer(size_t size) : data(new char[size]), end(data + size), cursor(data) {}
                };
            private:
                void writeToFile(const void * data, size_t size)
                {
                    LOCK(F_sync);

                   #if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
                    _fwrite_nolock(data, 1, size, F);
                   #elif defined(__linux__)
                    fwrite_unlocked(data, 1, size, F);
                   #else
                    fwrite(data, 1, size, F);
                   #endif
                }
                void worker()
                {
                    while (A && B)
                    {
                        if (hasReadyBuffer)
                        {
                            writeToFile(B->data, B->cursor - B->data);
                            B->cursor = B->data;
                            hasReadyBuffer = false;
                        }
                        else {
                            std::this_thread::sleep_for(std::chrono::microseconds{ 500 });
                        }
                    }
                }
                void flush(bool close)
                {
                    while (hasReadyBuffer) { std::this_thread::yield(); }
                    if (B->cursor > B->data) {
                        throw std::logic_error("flush: buffer B is supposed to be clear (but it isnt).");
                    }
                    // B is either already empty or was cleared after writing

                    std::swap(A, B);       // doesnt matter if A is full
                    hasReadyBuffer = true; // run file writing

                    while (hasReadyBuffer) { std::this_thread::yield(); }
                    if (B->cursor > B->data) {
                        throw std::logic_error("flush: buffer B is supposed to be clear (but it isnt).");
                    }
                    // A is either already empty or was cleared after writing

                    if (close) {
                        fclose(F);
                        F = nullptr;

                        delete A;
                        delete B;
                        A = B = nullptr;

                        if (W.joinable()) { W.join(); }
                    }
                }
            private:
                Buffer *A, *B;
                FILE * F;
                std::thread W;
                _utl::SpinLock F_sync;
                std::atomic_bool hasReadyBuffer;
            public:
                void changeFile(FILE * file)
                {
                    LOCK(F_sync);
                    if (F) { fclose(F); }
                    F = file;
                    if (file) { std::setvbuf(F, nullptr, _IONBF, 0); }
                }
                void write(const void * data, size_t size)
                {
                    if (A->cursor + size > A->end)
                    {
                        while (hasReadyBuffer) { std::this_thread::sleep_for(std::chrono::microseconds{ 500 }); }
                        std::swap(A, B);
                        hasReadyBuffer = true;
                    }
                    auto p = static_cast<const char*>(data);
                    auto end = p + size;
                    while (p < end) { *A->cursor++ = *p++; }
                }
                long tell()
                {
                    LOCK(F_sync);
                    return ftell(F);
                }
                void flush() { flush(false); }
                void close() { flush(true); }

                BackgroundWriter(size_t bufferSize, FILE * file = nullptr)
                    : A(new Buffer{ bufferSize / 2 })
                    , B(new Buffer{ (bufferSize + 1) / 2 })
                    , F(file)
                    , hasReadyBuffer(false)
                {
                    if (file) { std::setvbuf(F, nullptr, _IONBF, 0); }
                    W = std::thread{ &BackgroundWriter::worker, this };
                }
                ~BackgroundWriter() {
                    flush(true);
                    if (W.joinable()) { W.join(); }
                }
            };
        }
    }

    #define detail detail::rolling_file_writer

    class RollingFileWriter : public _utl::AbstractWriter {
    private:
        const std::string m_filePathPrefix;
        detail::BackgroundWriter m_sink;
        uint64_t m_maxSize;
        uint64_t m_maxSeconds;
        time_t m_rollTimepoint;
        std::vector<char> m_buffer;
        uint32_t (RollingFileWriter::*write_function)(const void*, uint32_t) = &RollingFileWriter::write_firstOpen;
    private:
        __forceinline const std::string & newFilePath()
        {
            static std::string path;

            path.clear();
            path.reserve(m_filePathPrefix.size() + 30);

            std::time_t time_ = std::time(nullptr);
            tm * tm_ = std::gmtime(&time_);

            path.append(m_filePathPrefix);
            path.resize(m_filePathPrefix.size() + 23);

            auto p = const_cast<char*>(path.data() + m_filePathPrefix.size());
            std::strftime(p, 24, "%Y-%m-%d.%H-%M-%S.txt", tm_);

            return path;
        }
        __forceinline std::string formatFilePathPrefix(const std::string & path, const std::string & prefix)
        {
            std::string result;
            result.reserve(path.size() + 1 + prefix.size() + 1);

            bool hasDelimiter = path.size() && ( path.back() == '\\' || path.back() == '/' );

            result.append(path.size() ? path : ".");
            result.append(hasDelimiter ? "" : "/");
            result.append(prefix);

            return result;
        }
        time_t calcRollTimepoint(int64_t maxSeconds)
        {
            #if defined(max)
             #undef max
             #define MAX_MACRO_IS_DEFINED
            #endif

            time_t t = time(nullptr);
            t = (maxSeconds > 0)
                ? t + maxSeconds
                : std::numeric_limits<time_t>::max();
            return t;

            #if defined(MAX_MACRO_IS_DEFINED)
             #define max(a,b) ((a) > (b) ? (a) : (b))
            #endif
        }
    private:
        uint32_t write_firstOpen(const void * data, uint32_t size)
        {
            m_rollTimepoint = calcRollTimepoint(m_maxSeconds);

            auto file = fopen(newFilePath().data(), "wb");
            m_sink.changeFile(file);

            bool rollingEnabled =
                (m_maxSeconds != uint64_t(-1)) ||
                (m_maxSize != uint64_t(-1));

            write_function = (rollingEnabled)
                ? &RollingFileWriter::write_rolling
                : &RollingFileWriter::write_simple;

            return (this->*write_function)(data, size);
        }
        uint32_t write_rolling(const void * data, uint32_t size)
        {
            if (time(nullptr) > m_rollTimepoint || static_cast<uint64_t>(m_sink.tell() + size > m_maxSize))
            {
                m_rollTimepoint = calcRollTimepoint(m_maxSeconds);
                auto file = fopen(newFilePath().data(), "wb");
                m_sink.changeFile(file);
            }
            return write_simple(data, size);
        }
        uint32_t write_simple(const void * data, uint32_t size)
        {
            m_sink.write(data, size);
            return size;
        }
    public:
        RollingFileWriter(const std::string & dirPath = "logs/", const std::string & namePrefix = "", uint64_t maxSize = -1, uint64_t maxSeconds = -1, uint32_t bufferSize = 256*1024)
            : m_filePathPrefix(formatFilePathPrefix(dirPath, namePrefix))
            , m_maxSize(maxSize)
            , m_maxSeconds(maxSeconds)
            , m_sink(bufferSize, nullptr)
        {}

        ~RollingFileWriter() override;

        void flush() override { m_sink.flush(); }
        uint32_t write(const void * data, uint32_t size) override { return (this->*write_function)(data, size); }
    };

    #undef detail

    RollingFileWriter::~RollingFileWriter() { m_sink.flush(); }
}
