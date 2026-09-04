#include "inverter_gateway/inverter/diagnostic_scanner.hpp"

#include <cinttypes>
#include <cstdio>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inverter_gateway/inverter/aohai_registers.hpp"
#include "inverter_gateway/transport/usb_rs485.hpp"

namespace inverter_gateway::inverter {
namespace {

constexpr char tag[] = "inverter_scan";
constexpr bool discovery_scan_enabled = true;
constexpr bool holding_change_scan_enabled = true;

void print_register_table(const std::uint16_t *values, std::uint16_t first,
                          std::uint16_t count)
{
    std::printf("\nRegisters:\nAddress  Decimal   Signed    Hex\n");
    std::printf("-------  --------  --------  ------\n");
    for (std::uint16_t index = 0; index < count; ++index) {
        std::printf("%7u  %8u  %8d  0x%04X\n", first + index, values[index],
                    static_cast<std::int16_t>(values[index]), values[index]);
    }
}

} // namespace

DiagnosticScanner::DiagnosticScanner(protocol::ModbusRtuClient &client)
    : client_(client)
{
}

void DiagnosticScanner::reset_connection_state()
{
    holding_baseline_captured_ = false;
    previous_holding_register_valid_.fill(false);
}

void DiagnosticScanner::run_cycle()
{
    if (discovery_scan_enabled) {
        scan_input_registers();
    } else {
        poll_basic_registers();
    }

    if (holding_change_scan_enabled && !transport::UsbRs485::disconnected()) {
        scan_holding_register_changes();
    }
}

void DiagnosticScanner::poll_basic_registers()
{
    std::printf("\n============================================================\n");
    if (!client_.read_input(basic_first_register, basic_register_count,
                            basic_registers_.data(), basic_registers_.size())) {
        ++failed_reads_;
    } else {
        ++successful_reads_;
        print_register_table(basic_registers_.data(), basic_first_register,
                             basic_register_count);
        std::printf("\nKnown FSC-12K values:\n");
        std::printf("Inverter status : %s\n",
                    aohai::inverter_status_name(basic_registers_[0]));
        std::printf("Inverter voltage: %.1f V\n", basic_registers_[2] * 0.1);
        std::printf("Inverter current: %.1f A\n",
                    static_cast<std::int16_t>(basic_registers_[5]) * 0.1);
        std::printf("DC bus voltage  : %.1f V\n", basic_registers_[8] * 0.1);
        std::printf("Inverter temp   : %.1f C\n",
                    static_cast<std::int16_t>(basic_registers_[10]) * 0.1);
        std::printf("Boost temp      : %.1f C\n",
                    static_cast<std::int16_t>(basic_registers_[11]) * 0.1);
        std::printf("LLC temp        : %.1f C\n",
                    static_cast<std::int16_t>(basic_registers_[12]) * 0.1);
    }
    std::printf("Communication statistics: %" PRIu32 " successful, %" PRIu32
                " failed\n", successful_reads_, failed_reads_);
}

void DiagnosticScanner::scan_input_registers()
{
    std::array<std::uint16_t, discovery_block_size> values{};
    std::uint16_t scanned = 0;
    std::uint16_t readable_blocks = 0;
    std::uint16_t unavailable_blocks = 0;

    discovery_registers_.fill(0);
    discovery_register_valid_.fill(false);

    std::printf("\n============================================================\n");
    std::printf("READ-ONLY INPUT-REGISTER DISCOVERY SCAN\n");
    std::printf("Scanning R%u-R%u with Modbus function 0x04.\n",
                discovery_first_register,
                discovery_first_register + discovery_register_count - 1);
    std::printf("Requests respect the documented minimum command interval.\n");

    while (scanned < discovery_register_count &&
           !transport::UsbRs485::disconnected()) {
        const std::uint16_t first = discovery_first_register + scanned;
        const std::uint16_t remaining = discovery_register_count - scanned;
        const std::uint16_t count = remaining < discovery_block_size
                                        ? remaining
                                        : discovery_block_size;

        std::printf("\n--- Discovery block %u-%u ---\n", first, first + count - 1);
        if (client_.read_input(first, count, values.data(), values.size())) {
            ++readable_blocks;
            print_register_table(values.data(), first, count);
            for (std::uint16_t index = 0; index < count; ++index) {
                const std::uint16_t offset = first + index - discovery_first_register;
                discovery_registers_[offset] = values[index];
                discovery_register_valid_[offset] = true;
            }
        } else {
            ++unavailable_blocks;
            ESP_LOGW(tag, "Input-register block %u-%u unavailable", first,
                     first + count - 1);
        }

        scanned += count;
    }

    std::printf("\nDiscovery scan finished: %u readable block(s), %u unavailable block(s).\n",
                readable_blocks, unavailable_blocks);
    if (readable_blocks != 0) {
        print_inquiry_summary();
    }
    std::printf("============================================================\n");
}

void DiagnosticScanner::print_inquiry_summary() const
{
    const auto valid = [this](std::uint16_t address) {
        return address >= discovery_first_register &&
               address < discovery_first_register + discovery_register_count &&
               discovery_register_valid_[address - discovery_first_register];
    };
    const auto value = [this](std::uint16_t address) {
        return discovery_registers_[address - discovery_first_register];
    };
    const auto signed_value = [&value](std::uint16_t address) {
        return static_cast<std::int16_t>(value(address));
    };

    std::printf("\n============================================================\n");
    std::printf("EXPLICIT FSC-12K INQUIRY RESULT\n");
    if (valid(0)) std::printf("Operating status       [R0]   : %s (raw %u)\n",
                              aohai::inverter_status_name(value(0)), value(0));
    if (valid(1)) std::printf("Grid-connect countdown [R1]   : %u s\n", value(1));
    if (valid(2)) std::printf("Inverter voltage       [R2]   : %.1f V\n", value(2) * 0.1);
    if (valid(5)) std::printf("Inverter current       [R5]   : %.1f A\n", signed_value(5) * 0.1);
    if (valid(8)) std::printf("DC-bus voltage         [R8]   : %.1f V\n", value(8) * 0.1);
    if (valid(10)) std::printf("Inverter temperature   [R10]  : %.1f C\n", signed_value(10) * 0.1);
    if (valid(11)) std::printf("Boost temperature      [R11]  : %.1f C\n", signed_value(11) * 0.1);
    if (valid(12)) std::printf("LLC temperature        [R12]  : %.1f C\n", signed_value(12) * 0.1);
    if (valid(13)) std::printf("Battery NTC temperature[R13]  : %.1f C\n", signed_value(13) * 0.1);
    if (valid(18)) std::printf("R-phase DC current     [R18]  : %d mA\n", signed_value(18));
    if (valid(32)) std::printf("Main fault code        [R32]  : %u\n", value(32));
    if (valid(33)) std::printf("Main warning code      [R33]  : %u\n", value(33));
    if (valid(35)) std::printf("Warning subcode        [R35]  : %u\n", value(35));
    if (valid(36)) std::printf("Device type            [R36]  : %u\n", value(36));
    if (valid(42)) std::printf("Grid voltage           [R42]  : %.1f V\n", value(42) * 0.1);
    if (valid(43)) std::printf("Grid current           [R43]  : %.1f A\n", signed_value(43) * 0.1);
    if (valid(51)) std::printf("Grid frequency         [R51]  : %.2f Hz\n", value(51) * 0.01);
    if (valid(52)) std::printf("Power factor           [R52]  : %.4f\n", signed_value(52) * 0.0001);
    if (valid(54)) std::printf("EPS frequency          [R54]  : %.2f Hz\n", value(54) * 0.01);
    if (valid(55)) std::printf("EPS voltage            [R55]  : %.1f V\n", value(55) * 0.1);
    if (valid(56)) std::printf("EPS current            [R56]  : %.1f A\n", value(56) * 0.1);
    if (valid(63)) std::printf("PV path count          [R63]  : %u\n", value(63));
    if (valid(64)) std::printf("PV1 voltage            [R64]  : %.1f V\n", value(64) * 0.1);
    if (valid(65)) std::printf("PV1 current            [R65]  : %.1f A\n", value(65) * 0.1);
    if (valid(66)) std::printf("PV2 voltage            [R66]  : %.1f V\n", value(66) * 0.1);
    if (valid(67)) std::printf("PV2 current            [R67]  : %.1f A\n", value(67) * 0.1);
    if (valid(96)) std::printf("Generator voltage R    [R96]  : %.1f V\n", value(96) * 0.1);
    if (valid(107) && valid(108)) {
        const std::uint32_t units = aohai::join_u32(value(107), value(108));
        std::printf("Total operation time   [R107:108]: %.1f h\n", units * (0.5 / 60.0));
    }

    std::printf("\nBATTERY / BMS\n");
    if (valid(125)) std::printf("Energy priority        [R125] : %s (raw %u)\n",
                                aohai::battery_priority_name(value(125)), value(125));
    if (valid(126)) std::printf("Battery type           [R126] : %s (raw %u)\n",
                                aohai::battery_type_name(value(126)), value(126));
    if (valid(127)) std::printf("Battery voltage        [R127] : %.1f V\n", value(127) * 0.1);
    if (valid(128)) std::printf("Battery SOC            [R128] : %.2f %%\n", value(128) * 0.01);
    if (valid(130)) std::printf("BMS status             [R130] : 0x%04X\n", value(130));
    if (valid(141)) std::printf("Battery current        [R141] : %.2f A\n", signed_value(141) * 0.01);
    if (valid(145)) std::printf("Rated battery capacity [R145] : %.1f Ah\n", value(145) * 0.1);
    if (valid(146)) std::printf("Remaining capacity     [R146] : %.1f Ah\n", value(146) * 0.1);
    if (valid(151)) std::printf("Battery cycles         [R151] : %u\n", value(151));
    if (valid(152)) std::printf("Battery SOH            [R152] : %u %%\n", value(152));

    std::printf("\nPV POWER\n");
    if (valid(250) && valid(251)) {
        const std::uint32_t raw = aohai::join_u32(value(250), value(251));
        std::printf("Total PV power         [R250:251]: %.1f W\n", raw * 0.1);
    }
    if (valid(252) && valid(253)) {
        const std::uint32_t raw = aohai::join_u32(value(252), value(253));
        std::printf("PV1 power              [R252:253]: %.1f W\n", raw * 0.1);
    }
    if (valid(254) && valid(255)) {
        const std::uint32_t raw = aohai::join_u32(value(254), value(255));
        std::printf("PV2 power              [R254:255]: %.1f W\n", raw * 0.1);
    }
    std::printf("============================================================\n");
}

void DiagnosticScanner::scan_holding_register_changes()
{
    std::array<std::uint16_t, holding_register_count> values{};

    std::printf("\n============================================================\n");
    std::printf("READ-ONLY FSC-12K HOLDING-REGISTER SCAN\n");
    std::printf("Scanning H%u-H%u with function 0x03. No writes are sent.\n",
                holding_first_register,
                holding_first_register + holding_register_count - 1);

    if (!client_.read_holding(holding_first_register, holding_register_count,
                              values.data(), values.size())) {
        ESP_LOGW(tag, "Holding-register block H%u-H%u unavailable",
                 holding_first_register,
                 holding_first_register + holding_register_count - 1);
        return;
    }

    print_register_table(values.data(), holding_first_register,
                         holding_register_count);
    constexpr std::uint16_t buzzer_offset =
        aohai::holding_buzzer - holding_first_register;
    const std::uint16_t buzzer_value = values[buzzer_offset];
    const char *buzzer_name = buzzer_value == 0 ? "Disabled" :
                              buzzer_value == 1 ? "Enabled" : "Unknown";
    std::printf("\nBuzzer [H207]: %s (raw %u, 0x%04X)\n", buzzer_name,
                buzzer_value, buzzer_value);

    if (!holding_baseline_captured_) {
        std::printf("CHANGE DETECTOR: baseline captured.\n");
    } else {
        bool found_change = false;
        std::printf("\nCHANGES SINCE PREVIOUS HOLDING POLL\n");
        for (std::uint16_t index = 0; index < holding_register_count; ++index) {
            if (previous_holding_register_valid_[index] &&
                previous_holding_registers_[index] != values[index]) {
                found_change = true;
                const std::uint16_t address = holding_first_register + index;
                std::printf("H%-3u: %5u (0x%04X) -> %5u (0x%04X)%s\n",
                            address, previous_holding_registers_[index],
                            previous_holding_registers_[index], values[index], values[index],
                            address == aohai::holding_buzzer ? "  <-- BUZZER" : "");
            }
        }
        if (!found_change) {
            std::printf("No holding-register changes detected.\n");
        }
    }

    previous_holding_registers_ = values;
    previous_holding_register_valid_.fill(true);
    holding_baseline_captured_ = true;
    std::printf("============================================================\n");
}

} // namespace inverter_gateway::inverter
