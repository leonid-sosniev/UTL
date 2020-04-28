#pragma once

#include <functional>
#include <utl/io/Reader.hpp>
#include <utl/io/Writer.hpp>
#include <assert.h>
#include <cstring>

namespace _utl
{

    class CircularBuffer : public _utl::AbstractReader, public _utl::AbstractWriter {
    private:
        std::function<void(void*)> deleter;
        uint32_t bufferSize;
        uint32_t dataSize;
        uint32_t putPos;
        uint32_t getPos;
        uint8_t buffer[0];
    private:
        inline uint32_t minimal(uint32_t a, uint32_t b) {
            return a < b ? a : b;
        }
        uint8_t * readContinuousBlock(uint8_t * dst, uint32_t dstSize)
        {
            auto readableSize = minimal(dstSize, dataSize);
            auto continuousBlockSize = minimal(bufferSize - getPos, readableSize);

            std::memcpy(dst, &buffer[getPos], continuousBlockSize);
            
            getPos += continuousBlockSize;
            getPos = (getPos == bufferSize) ? 0 : getPos;
            dataSize -= continuousBlockSize;

            return dst + continuousBlockSize;
        }
        const uint8_t * writeContinuousBlock(const uint8_t * src, uint32_t srcSize)
        {
            auto writableSize = minimal(srcSize, bufferSize - dataSize);
            auto continuousBlockSize = minimal(bufferSize - putPos, writableSize);

            std::memcpy(&buffer[putPos], src, continuousBlockSize);
            
            putPos += continuousBlockSize;
            putPos = (putPos == bufferSize) ? 0 : putPos;
            dataSize += continuousBlockSize;

            return src + continuousBlockSize;
        }
    private:
        CircularBuffer(uint32_t maxDataSize, std::function<void(void*)> deleter)
            : deleter(deleter)
            , bufferSize(maxDataSize)
            , dataSize(0)
            , getPos(0)
            , putPos(0)
        {}
    public:
        static inline uint32_t getMemorySizeRequired(uint32_t maxDataSize) {
            return sizeof(CircularBuffer) + maxDataSize;
        }
        static inline CircularBuffer * createInPlace(uint32_t blockSize, void * block, std::function<void(void*)> deleter)
        {
            if (sizeof(CircularBuffer) >= blockSize) {
                return nullptr;
            }
            uint32_t sz = blockSize - sizeof(CircularBuffer);
            return new (block) CircularBuffer(sz, deleter);
        }
        static inline CircularBuffer * create(uint32_t dataSize)
        {
            auto blockSize = sizeof(CircularBuffer) + dataSize;
            uint8_t * block = new uint8_t[blockSize];
            return createInPlace(
                blockSize,
                block,
                [](void*p){ delete[] static_cast<uint8_t*>(p); }
            );
        }
        virtual ~CircularBuffer() override { this->deleter(this); }

        virtual uint32_t read(void * data, uint32_t maxSize) override
        {
            auto dest = static_cast<uint8_t*>(data);
            auto destEnd = dest + maxSize;

            dest = readContinuousBlock(dest, destEnd - dest);
            dest = (dataSize == 0 || dest == destEnd) ? dest : readContinuousBlock(dest, destEnd - dest);

            return maxSize - (destEnd - dest);
        }
        virtual uint32_t write(const void * data, uint32_t size)
        {
            auto src = static_cast<const uint8_t*>(data);
            auto srcEnd = src + size;

            src = writeContinuousBlock(src, srcEnd - src);
            src = (dataSize == bufferSize || src == srcEnd) ? src : writeContinuousBlock(src, srcEnd - src);

            return size - (srcEnd - src);
        }
        virtual bool flush() override { return true; }

        uint32_t size() const { return dataSize; }
        uint32_t capacity() const { return bufferSize; }
    };

}
