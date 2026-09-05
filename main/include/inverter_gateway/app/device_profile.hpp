#pragma once

#include <cstdint>

namespace inverter_gateway::app {

enum class DeviceRole : std::uint8_t {
    unprovisioned,
    standalone_combined,
    parallel_coordinator,
    parallel_member,
    internet_gateway,
    coordinator_gateway_combined,
};

enum class SystemTopology : std::uint8_t {
    unknown,
    standalone_single_phase,
    standalone_native_three_phase,
    parallel_single_phase,
    parallel_three_phase_groups,
    parallel_native_three_phase,
};

enum class InverterRole : std::uint8_t {
    none,
    standalone,
    host,
    member,
};

enum class PhaseAssignment : std::uint8_t {
    none,
    parallel_single_phase,
    phase_1,
    phase_2,
    phase_3,
};

struct DeviceProfile {
    static constexpr std::uint8_t current_schema_version = 3;

    std::uint8_t schema_version = current_schema_version;
    DeviceRole device_role = DeviceRole::unprovisioned;
    SystemTopology topology = SystemTopology::unknown;
    InverterRole inverter_role = InverterRole::none;
    PhaseAssignment phase = PhaseAssignment::none;
    std::uint8_t expected_inverter_count = 0;
    std::uint8_t expected_member_count = 0;
    std::uint8_t logical_member_id = 0;
    std::uint8_t modbus_slave_address = 1;
    std::uint8_t espnow_channel = 0;
    std::uint8_t connected_pv_inputs = 2;
    bool battery_installed = true;
    bool bms_connected = true;
    bool generator_installed = false;
};

constexpr bool role_has_local_inverter(DeviceRole role)
{
    return role == DeviceRole::standalone_combined ||
           role == DeviceRole::parallel_coordinator ||
           role == DeviceRole::parallel_member ||
           role == DeviceRole::coordinator_gateway_combined;
}

constexpr bool role_uses_mqtt(DeviceRole role)
{
    return role == DeviceRole::standalone_combined ||
           role == DeviceRole::internet_gateway ||
           role == DeviceRole::coordinator_gateway_combined;
}

constexpr bool role_is_espnow_coordinator(DeviceRole role)
{
    return role == DeviceRole::parallel_coordinator ||
           role == DeviceRole::coordinator_gateway_combined;
}

constexpr bool role_is_espnow_member(DeviceRole role)
{
    return role == DeviceRole::parallel_member;
}

constexpr bool role_uses_espnow(DeviceRole role)
{
    return role_is_espnow_coordinator(role) || role_is_espnow_member(role);
}

constexpr bool role_uses_coordinator_link(DeviceRole role)
{
    return role == DeviceRole::parallel_coordinator ||
           role == DeviceRole::internet_gateway;
}

} // namespace inverter_gateway::app
