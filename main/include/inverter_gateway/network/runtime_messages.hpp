#pragma once

#include <array>
#include <cstdint>

#include "inverter_gateway/inverter/inverter_command.hpp"
#include "inverter_gateway/inverter/register_command.hpp"

namespace inverter_gateway::network {

inline constexpr std::size_t telemetry_register_count = 40;
enum class RemoteCommandKind : std::uint8_t {
    semantic,
    read_registers,
    write_registers,
};

enum class RemoteCommandOrigin : std::uint8_t {
    none,
    mqtt,
};

struct TelemetryMessage {
    std::uint8_t source_member_id = 0;
    std::uint8_t register_space = 0; // 0=holding, 1=input
    std::uint8_t connected_pv_inputs = 0;
    std::uint8_t phase_assignment = 0;
    std::uint16_t enabled_features = 0;
    std::uint16_t first_register = 0;
    std::uint16_t register_count = 0;
    std::uint32_t sequence = 0;
    std::int64_t timestamp_ms = 0;
    std::array<std::uint16_t, telemetry_register_count> registers{};
};

struct RoutedCommand {
    std::uint8_t target_member_id = 0;
    RemoteCommandKind kind = RemoteCommandKind::semantic;
    RemoteCommandOrigin origin = RemoteCommandOrigin::none;
    bool commissioning_interlock = false;
    inverter::LocalCommandRequest request{};
    inverter::RegisterCommandRequest register_request{};
};

struct RoutedCommandResult {
    std::uint8_t source_member_id = 0;
    RemoteCommandKind kind = RemoteCommandKind::semantic;
    inverter::LocalCommandResult result{};
    inverter::RegisterCommandResult register_result{};
};

class RuntimeMessageSink {
public:
    virtual ~RuntimeMessageSink() = default;
    virtual void on_telemetry(const TelemetryMessage &message) = 0;
    virtual void on_command(const RoutedCommand &command) = 0;
    virtual void on_command_result(const RoutedCommandResult &result) = 0;
};

} // namespace inverter_gateway::network
