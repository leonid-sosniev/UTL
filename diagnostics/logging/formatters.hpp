#pragma once

#include <utl/diagnostics/logging.hpp>

namespace _utl { namespace logging {

    class DummyEventFormatter final : public AbstractEventFormatter {
    public:
        virtual ~DummyEventFormatter() {}
        void formatEventAttributes(AbstractWriter &, const EventAttributes &) override {}
        void formatEvent(AbstractWriter &, const EventAttributes &, const Arg[]) override {}
    };
    class DummyTelemetryFormatter final : public AbstractTelemetryFormatter {
    public:
        virtual ~DummyTelemetryFormatter() {}
        void formatExpectedTypes(AbstractWriter &, uint16_t, const Arg::TypeID[]) override {}
        void formatValues(AbstractWriter &, uint16_t, const Arg[]) override {}
    };

} // logging
} // _utl
