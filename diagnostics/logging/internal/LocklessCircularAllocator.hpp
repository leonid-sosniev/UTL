#pragma once

#include <cstdint>
#include <atomic>
#include <memory>

namespace _utl {

    template<size_t SizeofItem> class LocklessCircularAllocator {
    private:
        using TItem = uint8_t[SizeofItem];
        std::atomic<uint32_t> m_actualLength;
        std::atomic<uint32_t> m_acquireIndex;
        std::atomic<uint32_t> m_releaseIndex;
        uint32_t const m_length;
        std::unique_ptr<TItem[]> m_buf;
    public:
        LocklessCircularAllocator(uint32_t length)
            : m_buf(new TItem[length])
            , m_length(length)
            , m_actualLength(length)
            , m_acquireIndex(0)
            , m_releaseIndex(0)
        {}
        
        /**
         * @brief Constructs a new Lockless Circular Allocator object
         * @pre The object is safe to move only right after its construction (before any method is called)
         * @param rhs Another object to move the data from
         */
        LocklessCircularAllocator(LocklessCircularAllocator && rhs)
            : m_buf(rhs.m_buf)
            , m_length(rhs.m_length)
            , m_actualLength(rhs.m_actualLength.load())
            , m_acquireIndex(rhs.m_acquireIndex.load())
            , m_releaseIndex(rhs.m_releaseIndex.load())
        {
            std::memset(&rhs, 0, sizeof(rhs));
        }
        LocklessCircularAllocator(const LocklessCircularAllocator &) = delete;

        LocklessCircularAllocator & operator=(LocklessCircularAllocator &&) = delete;
        LocklessCircularAllocator & operator=(const LocklessCircularAllocator &) = delete;

        inline bool isEmpty() const { return m_acquireIndex == m_releaseIndex; }
        inline void * acquire(uint32_t length)
        {
            TItem * result = nullptr;
            uint32_t acquireIndex_old = m_acquireIndex;
            for (;;)
            {
                uint32_t releaseIndex = m_releaseIndex;
                uint32_t acquireIndex_new = acquireIndex_old + length;
                if (acquireIndex_old < releaseIndex)
                {
                    if (acquireIndex_new >= releaseIndex) continue;
                    if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                    result = m_buf.get() + acquireIndex_old;
                    break;
                }
                else // releaseIndex <= acquireIndex_old
                {
                    if (acquireIndex_new < m_length) {
                        if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                        result = m_buf.get() + acquireIndex_old;
                        break;
                    } else {
                        m_actualLength.store(acquireIndex_old);
                        acquireIndex_new = length;
                        if (acquireIndex_new >= releaseIndex) continue;
                        if (m_acquireIndex.compare_exchange_weak(acquireIndex_old, acquireIndex_new) == false) continue;
                        result = m_buf.get();
                        break;
                    }
                }
            }
            return result;
        }
        inline void release(uint32_t length)
        {
            uint32_t acquireIndex;
            uint32_t releaseIndex_new;
            uint32_t releaseIndex_old = m_releaseIndex;
            for (;;)
            {
                uint32_t acquireIndex = m_acquireIndex;
                if (releaseIndex_old == acquireIndex) {
                    break;
                }
                uint32_t releaseIndex_new = releaseIndex_old + length;
                if (releaseIndex_old <= acquireIndex)
                {
                    if (releaseIndex_new > acquireIndex) continue;
                    if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                    break;
                }
                else // acquireIndex < releaseIndex_old
                {
                    if (releaseIndex_new < m_actualLength) {
                        if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                        break;
                    } else {
                        releaseIndex_new = length;
                        if (releaseIndex_new > acquireIndex) continue;
                        if (m_releaseIndex.compare_exchange_weak(releaseIndex_old, releaseIndex_new) == false) continue;
                        break;
                    }
                }
            }
        }
    };

} // _utl namespace