#pragma once

#include <cstdint>

#include <atomic>

namespace _utl {

    template<typename TItem> class LocklessQueue {
        std::atomic<uint32_t> m_usedCount;
        std::atomic<uint32_t> m_pushIndex;
        std::atomic<uint32_t> m_popIndex;
        uint32_t m_capacity;
        TItem * m_items;
        std::atomic<size_t> m_dbgEnqueueFailCount{0};
        std::atomic<size_t> m_dbgEnqueueCount{0};
        std::string m_debugName;
    public:
        LocklessQueue(uint32_t capacity, std::string debugName = "")
            : m_items(new TItem[capacity])
            , m_capacity(capacity)
            , m_usedCount(0)
            , m_popIndex(0)
            , m_pushIndex(0)
            , m_debugName(std::move(debugName))
        {}
        ~LocklessQueue()
        {
            std::cout << m_debugName << " enc fail/try: " << m_dbgEnqueueFailCount << "/" << m_dbgEnqueueCount << std::endl;
        }
        bool isEmpty() const { return !m_usedCount; }
        bool tryEnqueue(TItem && item)
        {
            uint32_t push = m_pushIndex;
            auto push_new = (push + 1) % m_capacity;
            if (push_new != m_popIndex && m_pushIndex.compare_exchange_weak(push, push_new))
            {
                m_items[push] = std::move(item);
                auto m_usedCount_debugValue = m_usedCount.fetch_add(1);
                assert(m_usedCount_debugValue < m_capacity);
                return true;
            }
            return false;
        }
        bool tryDequeue(TItem & dest)
        {
            uint32_t usedCount = m_usedCount;
            if (usedCount > 0 && m_usedCount.compare_exchange_weak(usedCount, usedCount - 1))
            {
                for (uint32_t pop;;)
                {
                    pop = m_popIndex;
                    if (m_popIndex.compare_exchange_weak(pop, (pop + 1) % m_capacity))
                    {
                        dest = std::move(m_items[pop]);
                        return true;
                    }
                }
            }
            return false;
        }
        inline void enqueue(TItem && item) {
            while (!tryEnqueue(std::move(item))) {
                ++m_dbgEnqueueFailCount;
            }
            ++m_dbgEnqueueCount;
        }
        inline TItem dequeue() {
            TItem item;
            while (!tryDequeue(item)) {}
            return std::move(item);
        }
    };

} // namespace _utl