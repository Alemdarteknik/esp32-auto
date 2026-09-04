#include "inverter_gateway/inverter/aohai_registers.hpp"

namespace inverter_gateway::inverter::aohai {

const char *inverter_status_name(std::uint16_t status)
{
    switch (status) {
    case 0: return "Waiting";
    case 1: return "On-grid";
    case 2: return "Off-grid";
    case 3: return "Fault";
    case 4: return "Firmware update";
    case 5: return "Bypass";
    case 6: return "Self-charge";
    case 7: return "Generator";
    default: return "Unknown";
    }
}

const char *battery_type_name(std::uint16_t type)
{
    switch (type) {
    case 0: return "Lead-acid";
    case 1: return "Lithium";
    case 2: return "User-defined 1";
    case 3: return "User-defined 2";
    case 4: return "User-defined 3";
    default: return "Unknown";
    }
}

const char *battery_priority_name(std::uint16_t priority)
{
    switch (priority) {
    case 0: return "Load priority";
    case 1: return "Battery priority";
    case 2: return "Grid priority";
    default: return "Unknown";
    }
}

} // namespace inverter_gateway::inverter::aohai
