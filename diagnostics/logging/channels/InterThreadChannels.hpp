#pragma once

#include <utl/diagnostics/logging.hpp>

#include <atomic>
#include <cassert>

namespace _utl { namespace logging {

namespace internal {

    #include <utl/diagnostics/logging/internal/LocklessQueue.hpp>
    template<typename TItem> using ConcurrentQueue = _utl::LocklessQueue<TItem>;

}

#define until(cond) while(!(cond))

    class InterThreadEventChannel final : public AbstractEventChannel {
        struct Trace {
            const EventAttributes * attrs;
            const Arg * args;
        };
        internal::ConcurrentQueue<Trace> m_eventQueue;
    public:
        InterThreadEventChannel(AbstractEventFormatter & formatter, AbstractWriter & writer, uint32_t argsAllocatorCapacity, uint32_t bufferSize)
            : AbstractEventChannel(formatter, writer, argsAllocatorCapacity)
            , m_eventQueue(bufferSize / sizeof(Trace))
        {}
        bool tryReceiveAndProcessEvent() final override
        {
            Trace trace;
            if (!m_eventQueue.tryDequeue(trace)) return false;
            if (!trace.args) {
                m_formatter.formatEventAttributes(m_writer, *trace.attrs);
                return tryReceiveAndProcessEvent();
            } else {
                m_formatter.formatEvent(m_writer, *trace.attrs, trace.args);
                releaseArgs(trace.attrs->argumentsExpected);
            }
            return true;
        }
    private:
        void sendEventAttributes_(const EventAttributes & attr) final override {
            m_eventQueue.enqueue(Trace{&attr,nullptr});
        }
        void sendEventOccurrence_(const EventAttributes & attr, const Arg args[]) final override {
            m_eventQueue.enqueue(Trace{&attr,args});
        }
    };

    class InterThreadTelemetryChannel final : public AbstractTelemetryChannel {
    private:
        internal::ConcurrentQueue<const Arg *> m_queue;
        uint16_t m_sampleLen;
    public:
        InterThreadTelemetryChannel(
                AbstractTelemetryFormatter & formatter, AbstractWriter & sink, uint32_t argsAllocatorCapacity,
                uint32_t bufferSize, uint16_t sampleLength, const Arg::TypeID * sampleTypes)
            : AbstractTelemetryChannel(formatter, sink, argsAllocatorCapacity)
            , m_queue(bufferSize / sizeof(Arg::Value))
            , m_sampleLen(0)
        {
            initializeAfterConstruction(sampleLength, sampleTypes);
        }
        [[nodiscard]] bool tryProcessSample() final override {
            const Arg * args;
            if (!m_queue.tryDequeue(args)) return false;
            m_formatter.formatValues(this->m_sink, args);
            releaseArgs(m_sampleLen);
            return true;
        }
        uint16_t sampleLength() const final override { return m_sampleLen; }
    private:
        void sendSampleTypes_(uint16_t sampleLength, const Arg::TypeID sampleTypes[]) final override {
            m_sampleLen = sampleLength;
            m_formatter.formatExpectedTypes(this->m_sink, m_sampleLen, sampleTypes);
        }
        void sendSample_(const Arg values[]) final override {
            m_queue.enqueue(std::move(values));
        }
    };

#undef until

} // namespace logging
} // namespace _utl
