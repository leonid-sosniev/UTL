#pragma once
#include <cstdint>
#include <cstdlib>

namespace _utl
{

    class AbstractWriter {
    public:
        /**
         * @param data -- pointer to input buffer
         * @param size -- byte count to be sent
         * @return -- number of successfuly written bytes
         * @throw  -- std::invalid_argument if (data == nullptr and size != 0)
         */
        virtual uint32_t write(const void * data, uint32_t size) = 0;

        /**
         * @return -- true if buffers (if any) were flushed successfuly
         */
        virtual bool flush() = 0;

        virtual ~AbstractWriter() {}
    };

    class DummyWriter : public AbstractWriter {
    public:
        virtual uint32_t write(const void *, uint32_t size) final override { return size; }
        virtual bool flush() final override { return true; }
    };

    class NullWriter : public AbstractWriter {
    public:
        virtual uint32_t write(const void *, uint32_t) final override { std::abort(); return 0; }
        virtual bool flush() final override { std::abort(); return false; }
    };

}
