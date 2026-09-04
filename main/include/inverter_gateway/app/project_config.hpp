#pragma once

// Central firmware defaults. Change these values before building when a
// product variant needs different hardware, timing, or commissioning defaults.

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

namespace inverter_gateway::app {

// Serial monitor
inline constexpr bool serial_monitor_output_default = false;

// Inverter Modbus and USB-RS485
inline constexpr std::uint32_t inverter_baud = 9600;
inline constexpr std::uint8_t default_modbus_slave_address = 1;
inline constexpr std::uint32_t modbus_response_timeout_ms = 1500;
inline constexpr std::uint32_t modbus_minimum_command_gap_ms = 850;
inline constexpr std::uint32_t usb_settle_time_ms = 750;
inline constexpr std::size_t usb_buffer_size = 512;
inline constexpr std::uint32_t usb_open_timeout_ms = 1000;
inline constexpr std::uint32_t usb_reconnect_delay_ms = 250;
inline constexpr std::uint32_t usb_configure_retry_ms = 500;
inline constexpr std::uint32_t inverter_loop_delay_ms = 50;

// Production polling
inline constexpr std::uint32_t critical_poll_interval_ms = 1000;
inline constexpr std::uint32_t fast_poll_interval_ms = 5000;
inline constexpr std::uint32_t normal_poll_interval_ms = 30000;
inline constexpr std::uint32_t slow_poll_interval_ms = 300000;
inline constexpr std::uint32_t inverter_state_freshness_ms = 5000;
inline constexpr std::uint32_t diagnostic_poll_interval_ms = 5000;

// Setup access point and webpage
inline constexpr char setup_ap_ssid_prefix[] = "INVERTER-";
inline constexpr char setup_ap_password[] = "inverter123";
inline constexpr char setup_page_url[] = "http://esp32.local";
inline constexpr std::uint8_t setup_ap_default_channel = 1;
inline constexpr std::uint8_t setup_ap_max_connections = 4;
inline constexpr std::uint16_t setup_wifi_scan_max_results = 20;
inline constexpr std::uint32_t setup_status_refresh_ms = 3000;
inline constexpr std::uint32_t setup_verification_check_ms = 500;
inline constexpr std::uint32_t setup_verification_stable_ms = 3000;
inline constexpr std::uint32_t mqtt_wifi_ready_wait_ms = 15000;
inline constexpr std::uint32_t setup_http_task_stack_size = 6144;
inline constexpr std::size_t setup_request_max_bytes = 2048;

// Runtime setup button
inline constexpr gpio_num_t provisioning_button_gpio = GPIO_NUM_0;
inline constexpr std::uint32_t provisioning_button_hold_ms = 3000;

// Separate coordinator-to-Internet-Gateway cable
inline constexpr int coordinator_uart_port = 1;
inline constexpr gpio_num_t coordinator_uart_tx_gpio = GPIO_NUM_17;
inline constexpr gpio_num_t coordinator_uart_rx_gpio = GPIO_NUM_18;
inline constexpr std::uint32_t coordinator_uart_baud = 115200;
inline constexpr std::uint32_t coordinator_heartbeat_interval_ms = 2000;
inline constexpr std::uint32_t coordinator_link_timeout_ms = 10000;

// ESP-NOW discovery
inline constexpr std::uint32_t espnow_announce_interval_ms = 5000;

static_assert(default_modbus_slave_address >= 1 &&
              default_modbus_slave_address <= 247,
              "The default Modbus address must be between 1 and 247");
static_assert(critical_poll_interval_ms > 0 &&
              fast_poll_interval_ms >= critical_poll_interval_ms &&
              normal_poll_interval_ms >= fast_poll_interval_ms &&
              slow_poll_interval_ms >= normal_poll_interval_ms,
              "Polling intervals must be nonzero and ordered from critical to slow");
static_assert(sizeof(setup_ap_password) - 1 >= 8 &&
              sizeof(setup_ap_password) - 1 <= 63,
              "The setup AP password must contain 8 to 63 characters");
static_assert(setup_ap_default_channel >= 1 && setup_ap_default_channel <= 14,
              "The setup AP channel must be between 1 and 14");
static_assert(setup_verification_check_ms > 0 &&
              setup_verification_stable_ms >= setup_verification_check_ms,
              "Setup verification timing must be nonzero and ordered");
static_assert(mqtt_wifi_ready_wait_ms > 0,
              "MQTT Wi-Fi readiness wait must be nonzero");

} // namespace inverter_gateway::app
