#pragma once

#include <functional>
#include <ostream>

#include <utl/io/Writer.hpp>
#include <utl/diagnostics/logging.hpp>

namespace _utl { namespace logging {

class FlatBufferWriter final : public AbstractWriter {
    uint8_t * m_block;
    uint8_t * m_blockEnd;
    uint8_t * m_cursor;
    std::function<void(void*)> m_deleter;
public:
    FlatBufferWriter(void * block, uint32_t blockSize, std::function<void(void*)> deleter = {})
        : m_block(static_cast<uint8_t*>(block))
        , m_blockEnd(static_cast<uint8_t*>(block) + blockSize)
        , m_cursor(static_cast<uint8_t*>(block))
        , m_deleter(deleter)
    {}
    FlatBufferWriter(FlatBufferWriter && rhs)
        : m_block(rhs.m_block)
        , m_blockEnd(rhs.m_blockEnd)
        , m_cursor(rhs.m_cursor)
        , m_deleter(std::move(rhs.m_deleter))
    {
        rhs.m_block = nullptr;
        rhs.m_blockEnd = nullptr;
        rhs.m_cursor = nullptr;
    }
    ~FlatBufferWriter() {
        if (m_deleter) {
            m_deleter(m_block);
        }
    }
    inline uint32_t write(const void * data, uint32_t size) final override {
        auto sz = std::min<uint32_t>(size, m_blockEnd - m_cursor);
        std::memcpy(m_cursor, data, sz);
        m_cursor += sz;
        return sz;
    }
    bool flush() final override {
        return true;
    }
    const void * position() const {
        return m_cursor;
    }
};

class StreamWriter final : public AbstractWriter {
private:
    std::ostream & m_stm;
public:
    StreamWriter(std::ostream & stm) : m_stm(stm)
    {}
    uint32_t write(const void * data, uint32_t size) final override {
        m_stm.write(static_cast<const char *>(data), size);
        return size;
    }
    bool flush() final override {
        return true;
    }
};

} // logging
} // _utl