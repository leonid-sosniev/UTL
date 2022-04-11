#pragma once
#include <stdint.h>

namespace _utl
{

    class AbstractReader {
    public:
        /**
         * @param data -- pointer to output buffer
         * @param size -- byte count to be sent
         * @return -- number of successfuly written bytes
         * @throw  -- std::invalid_argument if (data == nullptr and size != 0)
         */
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

    class NullReader : public AbstractReader {
    public:
        virtual uint32_t read(void * data, uint32_t maxSize) override
        {
            (void) data; (void) maxSize;
            std::abort(); return 0;
        }
    };

}