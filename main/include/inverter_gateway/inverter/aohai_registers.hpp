#pragma once

#include <cstdint>

namespace inverter_gateway::inverter::aohai {

inline constexpr std::uint16_t holding_inverter_on_off = 0;
inline constexpr std::uint16_t holding_battery_type = 125;
inline constexpr std::uint16_t holding_feature_flags = 201;
inline constexpr std::uint16_t inverter_status = 0;
inline constexpr std::uint16_t grid_connection_countdown = 1;
inline constexpr std::uint16_t inverter_voltage_r = 2;
inline constexpr std::uint16_t inverter_current_r = 5;
inline constexpr std::uint16_t dc_bus_voltage = 8;
inline constexpr std::uint16_t inverter_temperature = 10;
inline constexpr std::uint16_t boost_temperature = 11;
inline constexpr std::uint16_t llc_temperature = 12;
inline constexpr std::uint16_t grid_voltage_r = 42;
inline constexpr std::uint16_t grid_current_r = 43;
inline constexpr std::uint16_t grid_frequency = 51;
inline constexpr std::uint16_t grid_power_factor = 52;
inline constexpr std::uint16_t eps_frequency = 54;
inline constexpr std::uint16_t eps_voltage_r = 55;
inline constexpr std::uint16_t eps_current_r = 56;
inline constexpr std::uint16_t pv_path_count = 63;
inline constexpr std::uint16_t pv1_voltage = 64;
inline constexpr std::uint16_t pv1_current = 65;
inline constexpr std::uint16_t pv2_voltage = 66;
inline constexpr std::uint16_t pv2_current = 67;
inline constexpr std::uint16_t generator_voltage_r = 96;
inline constexpr std::uint16_t total_runtime_high = 107;
inline constexpr std::uint16_t total_runtime_low = 108;
inline constexpr std::uint16_t battery_priority = 125;
inline constexpr std::uint16_t battery_type = 126;
inline constexpr std::uint16_t battery_voltage = 127;
inline constexpr std::uint16_t battery_soc = 128;
inline constexpr std::uint16_t bms_status = 130;
inline constexpr std::uint16_t battery_current = 141;
inline constexpr std::uint16_t battery_soh = 152;
inline constexpr std::uint16_t total_pv_power_high = 250;
inline constexpr std::uint16_t total_pv_power_low = 251;
inline constexpr std::uint16_t pv1_power_high = 252;
inline constexpr std::uint16_t pv1_power_low = 253;
inline constexpr std::uint16_t pv2_power_high = 254;
inline constexpr std::uint16_t pv2_power_low = 255;

inline constexpr std::uint16_t holding_buzzer = 207;
inline constexpr std::uint16_t holding_overload_to_bypass = 208;
inline constexpr std::uint16_t holding_bluetooth = 231;

const char *inverter_status_name(std::uint16_t status);
const char *battery_type_name(std::uint16_t type);
const char *battery_priority_name(std::uint16_t priority);

constexpr std::uint32_t join_u32(std::uint16_t high, std::uint16_t low)
{
    return (static_cast<std::uint32_t>(high) << 16U) | low;
}

} // namespace inverter_gateway::inverter::aohai
