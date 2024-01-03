#pragma once

#include <utl/diagnostics/logging.hpp>

namespace _utl { namespace logging {

    class DummyEventFormatter final : public AbstractEventFormatter {
    private:
        void formatEventAttributes_(const EventAttributes &) override {}
        void formatEvent_(const EventAttributes &, const Arg[]) override {}
    };
    //class DummyTelemetryFormatter final : public AbstractTelemetryFormatter {
    //private:
    //    void formatExpectedTypes(uint16_t, const Arg::TypeID[]) override {}
    //    void formatValues(AbstractWriter &, const Arg[]) override {}
    //};

} // logging
} // _utl
