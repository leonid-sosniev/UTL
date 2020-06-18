#pragma once
#include <stdint.h>

namespace _utl
{

    class AbstractWriter {
    public:
        virtual uint32_t write(const void * data, uint32_t size) = 0;
        virtual bool flush() = 0;
        virtual ~AbstractWriter() {}
    };

    class DummyWriter : public AbstractWriter {
    public:
        virtual uint32_t write(const void * data, uint32_t size) override { (void) data; (void) size; return size; }
        virtual bool flush() override { return true; }
    };

    class NullWriter : public AbstractWriter {
    public:
        uint32_t write(const void * data, uint32_t size) override { std::abort(); (void) data; (void) size; return 0; }
        bool flush() override { std::abort(); return false; }
    };

}
