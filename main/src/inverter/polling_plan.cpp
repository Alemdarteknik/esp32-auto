#include "inverter_gateway/inverter/polling_plan.hpp"

namespace inverter_gateway::inverter {
namespace {

constexpr PollRequest requests[] = {
    {RegisterSpace::holding, 2, 1, PollCadence::boot_once, feature_none, "device-type"},
    {RegisterSpace::holding, 5, 15, PollCadence::boot_once, feature_none, "serial-number"},
    {RegisterSpace::holding, 28, 37, PollCadence::boot_once, feature_none, "versions-model-rated-power"},
    {RegisterSpace::holding, 3, 2, PollCadence::setup_once, feature_none, "modbus-configuration"},
    {RegisterSpace::holding, 20, 8, PollCadence::setup_once, feature_none, "communication-and-clock"},
    {RegisterSpace::holding, 68, 28, PollCadence::setup_once, feature_none, "grid-pv-ct-policy"},
    {RegisterSpace::holding, 100, 17, PollCadence::setup_once, feature_none, "startup-eps-protection"},
    {RegisterSpace::holding, 121, 103, PollCadence::setup_once, feature_none, "battery-schedules-parallel-generator"},
    {RegisterSpace::holding, 231, 5, PollCadence::setup_once, feature_none, "bluetooth-and-collector"},
    {RegisterSpace::holding, 250, 31, PollCadence::setup_once, feature_none, "grid-code-protection"},

    {RegisterSpace::input, 0, 40, PollCadence::critical, feature_none, "core-status-faults"},
    {RegisterSpace::input, 40, 28, PollCadence::fast, feature_none, "grid-eps-pv1-pv2"},
    {RegisterSpace::input, 68, 28, PollCadence::normal, feature_extended_pv, "pv3-pv16"},
    {RegisterSpace::input, 96, 7, PollCadence::normal, feature_generator, "generator-electrical"},
    {RegisterSpace::input, 106, 3, PollCadence::normal, feature_none, "fan-and-runtime"},
    {RegisterSpace::input, 109, 16, PollCadence::on_demand, feature_service, "dsp-debug"},
    {RegisterSpace::input, 125, 51, PollCadence::fast, feature_battery, "battery-and-bms"},
    {RegisterSpace::input, 186, 10, PollCadence::on_demand, feature_service, "bms-debug"},
    {RegisterSpace::input, 250, 107, PollCadence::fast, feature_none, "pv-ac-grid-load-battery-power"},
    {RegisterSpace::input, 357, 12, PollCadence::normal, feature_generator, "generator-power"},
    {RegisterSpace::input, 375, 56, PollCadence::slow, feature_none, "energy-counters"},
    {RegisterSpace::input, 450, 49, PollCadence::fast, feature_parallel, "parallel-system"},
};

} // namespace

PollPlanView default_poll_plan()
{
    return {requests, sizeof(requests) / sizeof(requests[0])};
}

bool poll_request_is_enabled(const PollRequest &request,
                             std::uint16_t enabled_features)
{
    return request.required_features == feature_none ||
           (enabled_features & request.required_features) == request.required_features;
}

const char *poll_cadence_name(PollCadence cadence)
{
    switch (cadence) {
    case PollCadence::boot_once: return "BOOT_ONCE";
    case PollCadence::setup_once: return "SETUP_ONCE";
    case PollCadence::critical: return "CRITICAL";
    case PollCadence::fast: return "FAST";
    case PollCadence::normal: return "NORMAL";
    case PollCadence::slow: return "SLOW";
    case PollCadence::on_demand: return "ON_DEMAND";
    default: return "UNKNOWN";
    }
}

} // namespace inverter_gateway::inverter
