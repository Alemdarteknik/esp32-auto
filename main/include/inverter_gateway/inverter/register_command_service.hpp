#pragma once

#include "inverter_gateway/inverter/inverter_command.hpp"
#include "inverter_gateway/inverter/register_command.hpp"
#include "inverter_gateway/protocol/modbus_rtu.hpp"

namespace inverter_gateway::inverter {

class RegisterCommandService {
public:
    explicit RegisterCommandService(protocol::ModbusRtuClient &client);
    RegisterCommandResult execute(const RegisterCommandRequest &request,
                                  const InverterStateSnapshot &state,
                                  bool write_operation);

private:
    RegisterCommandResult execute_read(const RegisterCommandRequest &request);
    RegisterCommandResult execute_write(const RegisterCommandRequest &request,
                                        const InverterStateSnapshot &state);
    protocol::ModbusRtuClient &client_;
};

} // namespace inverter_gateway::inverter
