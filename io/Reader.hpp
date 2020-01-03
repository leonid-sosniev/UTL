#pragma once
#include <stdint.h>

namespace _utl
{

    class AbstractReader {
    public:
        virtual uint32_t read(void * data, uint32_t maxSize) = 0;
        virtual ~AbstractReader() {}
    };

    class DummyReader : public AbstractReader {
    private:
        uint32_t m_reportCount;
    public:
        DummyReader(bool reportZeroBytes) : m_reportCount(reportZeroBytes ? 0 : uint32_t(-1))
        {}
        virtual uint32_t read(void * data, uint32_t maxSize) override {
            (void) data;
            return (maxSize < m_reportCount) ? maxSize : m_reportCount;
        }
    };

}