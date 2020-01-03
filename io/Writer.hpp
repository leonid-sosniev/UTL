#pragma once
#include <stdint.h>

namespace _utl
{

    class AbstractWriter {
    public:
        virtual void write(const void * data, uint32_t size) = 0;
        virtual void flush() = 0;
        virtual ~AbstractWriter() {}
    };

    class DummyWriter : public AbstractWriter {
    public:
        virtual void write(const void * data, uint32_t size) override { (void) data; (void) size; }
        virtual void flush() override {}
    };

    class NullWriter : public AbstractWriter {
    public:
        void write(const void * data, uint32_t size) override { std::abort(); (void) data; (void) size; }
        void flush() override { std::abort(); }
    };

}
