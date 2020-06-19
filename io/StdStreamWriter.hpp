#pragma once

#include <utl/io/Writer.hpp>
#include <iostream>
#include <exception>
#include <cassert>

namespace _utl
{
    class StdStreamWriter : public _utl::AbstractWriter {
    private:
        std::ostream * m_stream;
    public:
        StdStreamWriter(std::ostream * stream = &std::cerr) : m_stream(stream) {
            if (!stream) {
                throw std::invalid_argument{"stream is null"};
            }
        }
        ~StdStreamWriter() override;

        uint32_t write(const void * data, uint32_t size) override
        {
            assert(m_stream);
            if (!size) { return 0; }
            if (!data) { throw std::invalid_argument{"cannot write null data"}; }

            auto posBefore = m_stream->tellp();
            m_stream->write(static_cast<const char*>(data), static_cast<std::streamsize>(size)).tellp();
            auto posAfter = m_stream->tellp();
            return posAfter - posBefore;
        }
        bool flush() override {
            assert(m_stream);
            m_stream->flush();
            return !(m_stream->failbit || m_stream->badbit);
        }
    };

    StdStreamWriter::~StdStreamWriter() {}
}
