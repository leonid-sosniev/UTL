#pragma once

#include <utl/io/Writer.hpp>
#include <utl/threading/Spinlock.hpp>
#include <stdio.h>
#include <ctime>
#include <limits>
#include <thread>

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#   define __forceinline __forceinline
#elif defined(__linux__)
#   define __forceinline __attribute__((always_inline))
#else
#   define __forceinline inline
#endif

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
                Buffer *A, *B;
                FILE * F;
                std::thread W;
                _utl::SpinLock fileSync;
                std::atomic_bool writtingBufferB, workerActive;
            private:
                inline bool BUF_isEmpty(const Buffer * buf) { return buf->cursor == buf->data; }
                inline bool BUF_isFull(const Buffer * buf) { return  buf->cursor == buf->end; }
                inline uint32_t BUF_capacity(const Buffer * buf) { return buf->end - buf->cursor; }
                inline uint32_t BUF_dataSize(const Buffer * buf) const { return buf->cursor - buf->data; }
                inline uint32_t BUF_freeSize(const Buffer * buf) const { return buf->end - buf->cursor; }
                inline uint32_t BUF_appendPart(Buffer * buf, const void * data, uint32_t size)
                {
                    uint32_t appendable = (BUF_freeSize(buf) < size) ? BUF_freeSize(buf) : size;
                    std::memcpy(buf->cursor, data, appendable);
                    buf->cursor += appendable;
                    return appendable;
                }
                inline void BUF_writeToFile(Buffer * buf)
                {
                    LOCK(fileSync);

                    #if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
                    _fwrite_nolock(buf->data, 1, BUF_dataSize(buf), F);
                    #elif defined(__linux__)
                    fwrite_unlocked(buf->data, 1, BUF_dataSize(buf), F);
                    #else
                    fwrite(buf->data, 1, BUF_dataSize(buf), F);
                    #endif

                    buf->cursor = buf->data;
                }
                void worker()
                {
                    while (workerActive)
                    {
                        if (writtingBufferB)
                        {
                            if (!F) {
                                std::abort();
                            }
                            BUF_writeToFile(B);
                            writtingBufferB = false;
                        }
                        else {
                            std::this_thread::sleep_for(std::chrono::microseconds{ 500 });
                        }
                    }
                }
            private:
                void flush(bool close)
                {
                    if (!F) { return; }

                    while (writtingBufferB) { std::this_thread::yield(); }
                    if (BUF_isEmpty(B) == false) {
                        throw std::logic_error("flush: buffer B is supposed to be clear (but it isnt).");
                    }
                    std::swap(A, B);      // doesnt matter if A is full
                    writtingBufferB = true; // run file writing

                    while (writtingBufferB) { std::this_thread::yield(); }
                    if (BUF_isEmpty(B) == false) {
                        throw std::logic_error("flush: buffer B is supposed to be clear (but it isnt).");
                    }
                    if (close)
                    {
                        fileSync.lock();
                        workerActive = false;
                        fclose(F); F = nullptr;
                        delete A;  A = nullptr;
                        delete B;  B = nullptr;
                        fileSync.unlock();
                        if (W.joinable()) { W.join(); }
                    }
                }
            public:
                void changeFile(FILE * file)
                {
                    LOCK(fileSync);
                    if (F) { fclose(F); }
                    if (F = file) {
                        std::setvbuf(F, nullptr, _IONBF, 0);
                    }
                    else {
                        throw std::runtime_error("changeFile: failed to open file.");
                    }
                }
                void write(const void * data, size_t size)
                {
                    register const char * p = static_cast<const char *>(data);
                    register const char * end = p + size;

                    for (;;)
                    {
                        p += BUF_appendPart(A, p, end-p);
                        if (end > p) {
                            while (writtingBufferB) { std::this_thread::sleep_for(std::chrono::microseconds{ 500 }); }
                            std::swap(A, B);
                            writtingBufferB = true;
                        }
                        else {
                            writtingBufferB = true;
                            return;
                        }
                    }
                }
                long tell()
                {
                    LOCK(fileSync);
                    return ftell(F);
                }
                void flush() { flush(false); }
                void close() { flush(true); }

                BackgroundWriter(size_t bufferSize, FILE * file = nullptr)
                    : A(new Buffer{ bufferSize / 2 })
                    , B(new Buffer{ (bufferSize + 1) / 2 })
                    , F(file)
                    , writtingBufferB(false)
                    , workerActive(true)
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

    RollingFileWriter::~RollingFileWriter() { m_sink.close(); }
}

#undef __forceinline