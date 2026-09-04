#pragma once

#include <array>
#include <cstdint>

#include "inverter_gateway/protocol/modbus_rtu.hpp"

namespace inverter_gateway::inverter {

class DiagnosticScanner {
public:
    explicit DiagnosticScanner(protocol::ModbusRtuClient &client);

    void reset_connection_state();
    void run_cycle();

private:
    static constexpr std::uint16_t basic_first_register = 0;
    static constexpr std::uint16_t basic_register_count = 20;
    static constexpr std::uint16_t discovery_first_register = 0;
    static constexpr std::uint16_t discovery_register_count = 256;
    static constexpr std::uint16_t discovery_block_size = 20;
    static constexpr std::uint16_t holding_first_register = 200;
    static constexpr std::uint16_t holding_register_count = 20;

    void scan_input_registers();
    void scan_holding_register_changes();
    void poll_basic_registers();
    void print_inquiry_summary() const;

    protocol::ModbusRtuClient &client_;
    std::array<std::uint16_t, basic_register_count> basic_registers_{};
    std::array<std::uint16_t, discovery_register_count> discovery_registers_{};
    std::array<bool, discovery_register_count> discovery_register_valid_{};
    std::array<std::uint16_t, holding_register_count> previous_holding_registers_{};
    std::array<bool, holding_register_count> previous_holding_register_valid_{};
    bool holding_baseline_captured_ = false;
    std::uint32_t successful_reads_ = 0;
    std::uint32_t failed_reads_ = 0;
};

} // namespace inverter_gateway::inverter
