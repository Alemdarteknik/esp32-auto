#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "usb/vcp.hpp"

namespace inverter_gateway::protocol {

enum class ModbusFunction : std::uint8_t {
    read_holding_registers = 0x03,
    read_input_registers = 0x04,
    write_single_register = 0x06,
    write_multiple_registers = 0x10,
};

enum class ModbusWriteStatus : std::uint8_t {
    success,
    invalid_argument,
    transmit_failed,
    disconnected,
    timeout,
    malformed_response,
    crc_error,
    exception_response,
    echo_mismatch,
};

struct ModbusWriteResult {
    ModbusWriteStatus status = ModbusWriteStatus::invalid_argument;
    std::uint8_t exception_code = 0;

    constexpr bool ok() const { return status == ModbusWriteStatus::success; }
};

std::uint16_t calculate_modbus_crc(const std::uint8_t *data, std::size_t length);

class ModbusRtuClient {
public:
    ModbusRtuClient(CdcAcmDevice &device, std::uint8_t slave_address,
                    std::uint32_t response_timeout_ms,
                    std::uint32_t minimum_command_gap_ms);

    bool read_holding(std::uint16_t first_register, std::uint16_t register_count,
                      std::uint16_t *destination, std::size_t destination_capacity);
    bool read_input(std::uint16_t first_register, std::uint16_t register_count,
                    std::uint16_t *destination, std::size_t destination_capacity);
    ModbusWriteResult write_single(std::uint16_t register_address,
                                   std::uint16_t value);
    ModbusWriteResult write_multiple(std::uint16_t first_register,
                                     std::uint16_t register_count,
                                     const std::uint16_t *values);

private:
    bool read(ModbusFunction function, std::uint16_t first_register,
              std::uint16_t register_count, std::uint16_t *destination,
              std::size_t destination_capacity);
    bool send_read_request(ModbusFunction function, std::uint16_t first_register,
                           std::uint16_t register_count);
    bool receive_read_response(ModbusFunction expected_function,
                               std::uint16_t requested_register_count,
                               std::uint16_t *destination,
                               std::size_t destination_capacity);
    ModbusWriteResult receive_write_single_response(std::uint16_t register_address,
                                                     std::uint16_t value);
    ModbusWriteResult receive_write_multiple_response(std::uint16_t first_register,
                                                       std::uint16_t register_count);
    void wait_for_command_slot();

    CdcAcmDevice &device_;
    std::uint8_t slave_address_;
    std::uint32_t response_timeout_ms_;
    TickType_t minimum_command_gap_ticks_;
    TickType_t last_command_tick_ = 0;
    bool command_has_been_sent_ = false;
};

} // namespace inverter_gateway::protocol
