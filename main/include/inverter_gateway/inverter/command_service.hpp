#pragma once

#include <cstdint>

#include "inverter_gateway/inverter/inverter_command.hpp"
#include "inverter_gateway/protocol/modbus_rtu.hpp"

namespace inverter_gateway::inverter {

class CommandService {
public:
    explicit CommandService(protocol::ModbusRtuClient &client);

    LocalCommandResult execute(const LocalCommandRequest &request,
                               const InverterStateSnapshot &state);

private:
    LocalCommandResult execute_boolean_register(const LocalCommandRequest &request,
                                                WriteGuard guard,
                                                std::uint16_t register_address);
    LocalCommandResult execute_feature_bit(const LocalCommandRequest &request,
                                           const InverterStateSnapshot &state,
                                           std::uint8_t bit);
    LocalCommandResult execute_battery_type(const LocalCommandRequest &request,
                                            const InverterStateSnapshot &state);
    LocalCommandResult execute_inverter_enabled(const LocalCommandRequest &request,
                                                const InverterStateSnapshot &state);
    LocalCommandResult write_and_verify(const LocalCommandRequest &request,
                                        WriteGuard guard,
                                        std::uint16_t register_address,
                                        std::uint16_t register_value,
                                        bool previous_value_valid,
                                        std::uint16_t previous_value);
    bool read_holding(std::uint16_t address, std::uint16_t &value);
    bool read_status(std::uint16_t &value);

    protocol::ModbusRtuClient &client_;
};

} // namespace inverter_gateway::inverter
