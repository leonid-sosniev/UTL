#pragma once

#include <utl/io/Writer.hpp>
#include <stdio.h>
#include <ctime>
#include <limits>

namespace _utl
{
    class RollingFileWriter : public _utl::AbstractWriter {
    private:
        const std::string m_filePathPrefix;
        FILE * m_sink;
        uint64_t m_maxSize;
        uint64_t m_maxSeconds;
        time_t m_rollTimepoint;
        std::vector<char> m_buffer;
    private:
        const std::string & newFilePath()
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
        static std::string formatAsDirpath(const std::string & path)
        {
            return (path.back() == '\\' || path.back() == '/') ? path : path + "/";
        }
        static time_t calcRollTimepoint(int64_t maxSeconds)
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
        void openStream(time_t rollTimepoint)
        {
            m_rollTimepoint = rollTimepoint;
            m_sink = fopen(newFilePath().data(), "wb");
            if (!m_sink) {
                throw std::runtime_error{"Could not open file for RollingFileWriter"};
            }
            std::setvbuf(m_sink, m_buffer.data(), _IOFBF, m_buffer.size());
        }
    public:
        RollingFileWriter(const std::string & dirPath = "logs/", const std::string & namePrefix = "", uint64_t maxSize = -1, uint64_t maxSeconds = -1, uint32_t bufferSize = 256*1024)
            : m_sink()
            , m_filePathPrefix(formatAsDirpath(dirPath) + namePrefix)
            , m_maxSize(maxSize)
            , m_maxSeconds(maxSeconds)
        {
            m_buffer.resize(bufferSize);
        }

        ~RollingFileWriter() override;

        void flush() override {
            fflush(m_sink);
        }
        void write(const void * data, uint32_t size) override
        {
            if (!m_sink) {
                time_t tp = calcRollTimepoint(m_maxSeconds);
                openStream(tp);
            }
            else if (time(nullptr) > m_rollTimepoint || static_cast<uint64_t>(ftell(m_sink)) + size > m_maxSize) {
                std::fclose(m_sink);
                time_t tp = calcRollTimepoint(m_maxSeconds);
                openStream(tp);
            }
            auto chars = static_cast<const char*>(data);

            #if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
                _fwrite_nolock(chars, 1, size, m_sink);
            #elif defined(__linux__)
                fwrite_unlocked(chars, 1, size, m_sink);
            #else
                fwrite(chars, 1, size, m_sink);
            #endif
        }
    };

    RollingFileWriter::~RollingFileWriter() { fclose(m_sink); }
}
