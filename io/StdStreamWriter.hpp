#pragma once

#include <utl/io/Writer.hpp>
#include <iostream>

namespace _utl
{
    class StdStreamWriter : public _utl::AbstractWriter {
    private:
        std::ostream * m_stream;
    public:
        StdStreamWriter(std::ostream * stream = &std::cerr) : m_stream(stream)
        {}
        ~StdStreamWriter() override;

        uint32_t write(const void * data, uint32_t size) override
        {
            auto posBefore = m_stream->tellp();
            m_stream->write(static_cast<const char*>(data), static_cast<int>(size)).tellp();
            auto posAfter = m_stream->tellp();
            return posAfter - posBefore;
        }
        bool flush() override {
            m_stream->flush();
            return !(m_stream->failbit || m_stream->badbit);
        }
    };

    StdStreamWriter::~StdStreamWriter() {}
}
