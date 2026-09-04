#include "inverter_gateway/network/coordinator_link.hpp"

#include <array>
#include <cstring>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "inverter_gateway/app/project_config.hpp"
#include "inverter_gateway/protocol/modbus_rtu.hpp"

namespace inverter_gateway::network {
namespace {

constexpr char tag[] = "coord_link";
constexpr std::uint16_t magic = 0x584d;
constexpr std::uint8_t version = 2;
constexpr std::uint8_t type_heartbeat = 1;
constexpr std::uint8_t type_telemetry = 2;
constexpr std::uint8_t type_command = 3;
constexpr std::uint8_t type_command_result = 4;
constexpr std::size_t maximum_payload = 256;

#pragma pack(push, 1)
struct Header {
    std::uint16_t magic_value;
    std::uint8_t protocol_version;
    std::uint8_t type;
    std::uint16_t payload_size;
    std::uint32_t site_hash;
    std::uint16_t crc;
};

struct IdentityPayload {
    std::uint8_t role;
    char site_id[app::max_site_id_length + 1];
};
#pragma pack(pop)

struct OutboundFrame {
    std::uint8_t type;
    std::uint16_t size;
    std::uint8_t payload[maximum_payload];
};

std::uint32_t site_hash(const char *text)
{
    std::uint32_t hash = 2166136261U;
    while (text != nullptr && *text != '\0') {
        hash = (hash ^ static_cast<std::uint8_t>(*text++)) * 16777619U;
    }
    return hash;
}

std::uint16_t frame_crc(std::uint8_t type, std::uint32_t site,
                        const void *payload, std::uint16_t size)
{
    std::array<std::uint8_t, 8 + maximum_payload> bytes{};
    bytes[0] = version;
    bytes[1] = type;
    bytes[2] = static_cast<std::uint8_t>(size);
    bytes[3] = static_cast<std::uint8_t>(size >> 8U);
    bytes[4] = static_cast<std::uint8_t>(site);
    bytes[5] = static_cast<std::uint8_t>(site >> 8U);
    bytes[6] = static_cast<std::uint8_t>(site >> 16U);
    bytes[7] = static_cast<std::uint8_t>(site >> 24U);
    if (size != 0) std::memcpy(bytes.data() + 8, payload, size);
    return inverter_gateway::protocol::calculate_modbus_crc(bytes.data(), 8 + size);
}

} // namespace

CoordinatorLink::CoordinatorLink(app::SystemConfig &config,
                                 RuntimeMessageSink &sink)
    : config_(config), sink_(sink)
{
}

esp_err_t CoordinatorLink::start()
{
    if (started_) return ESP_OK;
    uart_config_t uart_config{};
    uart_config.baud_rate = inverter_gateway::app::coordinator_uart_baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;
    const auto port = static_cast<uart_port_t>(inverter_gateway::app::coordinator_uart_port);
    ESP_RETURN_ON_ERROR(uart_param_config(port, &uart_config), tag, "UART config");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, inverter_gateway::app::coordinator_uart_tx_gpio,
                                     inverter_gateway::app::coordinator_uart_rx_gpio,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        tag, "UART pins");
    ESP_RETURN_ON_ERROR(uart_driver_install(port, 1024, 1024, 0, nullptr, 0),
                        tag, "UART driver");
    send_mutex_ = xSemaphoreCreateMutex();
    if (send_mutex_ == nullptr) {
        uart_driver_delete(port);
        return ESP_ERR_NO_MEM;
    }
    transmit_queue_ = xQueueCreate(32, sizeof(OutboundFrame));
    if (transmit_queue_ == nullptr) {
        vSemaphoreDelete(send_mutex_);
        send_mutex_ = nullptr;
        uart_driver_delete(port);
        return ESP_ERR_NO_MEM;
    }
    started_ = true;
    if (xTaskCreate(worker_task, "coord_link", 4096, this, 5, &task_) != pdPASS) {
        stop();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void CoordinatorLink::stop()
{
    started_ = false;
    if (task_ != nullptr) {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    uart_driver_delete(static_cast<uart_port_t>(inverter_gateway::app::coordinator_uart_port));
    if (send_mutex_ != nullptr) {
        vSemaphoreDelete(send_mutex_);
        send_mutex_ = nullptr;
    }
    if (transmit_queue_ != nullptr) {
        vQueueDelete(transmit_queue_);
        transmit_queue_ = nullptr;
    }
}

bool CoordinatorLink::connected() const
{
    const auto last = last_receive_tick_.load();
    return last != 0 && xTaskGetTickCount() - last <
                            pdMS_TO_TICKS(app::coordinator_link_timeout_ms);
}

esp_err_t CoordinatorLink::send_telemetry(const TelemetryMessage &message)
{
    return enqueue(type_telemetry, &message, sizeof(message));
}

esp_err_t CoordinatorLink::send_command(const RoutedCommand &command)
{
    return enqueue(type_command, &command, sizeof(command));
}

esp_err_t CoordinatorLink::send_command_result(const RoutedCommandResult &result)
{
    return enqueue(type_command_result, &result, sizeof(result));
}

esp_err_t CoordinatorLink::enqueue(std::uint8_t type, const void *payload,
                                   std::uint16_t size)
{
    if (!started_ || transmit_queue_ == nullptr || size > maximum_payload) {
        return ESP_ERR_INVALID_STATE;
    }
    OutboundFrame frame{};
    frame.type = type;
    frame.size = size;
    if (size != 0) std::memcpy(frame.payload, payload, size);
    return xQueueSend(transmit_queue_, &frame, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t CoordinatorLink::send(std::uint8_t type, const void *payload,
                                std::uint16_t size)
{
    if (!started_ || size > maximum_payload) return ESP_ERR_INVALID_STATE;
    std::array<std::uint8_t, sizeof(Header) + maximum_payload> frame{};
    const auto current_site_hash = site_hash(config_.site_id);
    Header header{magic, version, type, size, current_site_hash, 0};
    std::memcpy(frame.data(), &header, sizeof(header));
    if (size != 0) std::memcpy(frame.data() + sizeof(header), payload, size);
    header.crc = frame_crc(type, current_site_hash, payload, size);
    std::memcpy(frame.data(), &header, sizeof(header));
    if (xSemaphoreTake(send_mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    const int written = uart_write_bytes(
        static_cast<uart_port_t>(inverter_gateway::app::coordinator_uart_port), frame.data(),
        sizeof(header) + size);
    xSemaphoreGive(send_mutex_);
    return written == static_cast<int>(sizeof(header) + size) ? ESP_OK : ESP_FAIL;
}

void CoordinatorLink::worker_task(void *argument)
{
    auto *self = static_cast<CoordinatorLink *>(argument);
    if (self != nullptr) self->run();
    vTaskDelete(nullptr);
}

void CoordinatorLink::run()
{
    const auto port = static_cast<uart_port_t>(inverter_gateway::app::coordinator_uart_port);
    std::array<std::uint8_t, maximum_payload> payload{};
    std::int64_t last_heartbeat_ms = 0;
    OutboundFrame outbound{};
    while (started_) {
        const std::int64_t now = esp_timer_get_time() / 1000;
        if (now - last_heartbeat_ms >= app::coordinator_heartbeat_interval_ms) {
            IdentityPayload identity{};
            identity.role = static_cast<std::uint8_t>(config_.profile.device_role);
            std::strncpy(identity.site_id, config_.site_id,
                         sizeof(identity.site_id) - 1);
            send(type_heartbeat, &identity, sizeof(identity));
            last_heartbeat_ms = now;
        }
        if (connected() &&
            xQueueReceive(transmit_queue_, &outbound, 0) == pdTRUE) {
            send(outbound.type, outbound.payload, outbound.size);
        }
        std::uint8_t first = 0;
        if (uart_read_bytes(port, &first, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (first != static_cast<std::uint8_t>(magic & 0xff)) continue;
        Header header{};
        reinterpret_cast<std::uint8_t *>(&header)[0] = first;
        const int header_tail = uart_read_bytes(port,
            reinterpret_cast<std::uint8_t *>(&header) + 1, sizeof(header) - 1,
            pdMS_TO_TICKS(100));
        if (header_tail != static_cast<int>(sizeof(header) - 1) ||
            header.magic_value != magic || header.protocol_version != version ||
            header.payload_size > payload.size()) continue;
        if (header.payload_size != 0 &&
            uart_read_bytes(port, payload.data(), header.payload_size,
                            pdMS_TO_TICKS(250)) != header.payload_size) continue;
        const auto crc = frame_crc(header.type, header.site_hash,
                                   payload.data(), header.payload_size);
        if (crc != header.crc) continue;
        if (header.site_hash != site_hash(config_.site_id)) {
            const bool can_adopt =
                !config_.provisioned &&
                config_.profile.device_role == app::DeviceRole::internet_gateway &&
                header.type == type_heartbeat &&
                header.payload_size == sizeof(IdentityPayload);
            if (!can_adopt) continue;
            IdentityPayload identity{};
            std::memcpy(&identity, payload.data(), sizeof(identity));
            if (identity.role != static_cast<std::uint8_t>(app::DeviceRole::parallel_coordinator) ||
                std::memchr(identity.site_id, '\0', sizeof(identity.site_id)) == nullptr ||
                !app::valid_site_id(identity.site_id)) continue;
            std::strncpy(config_.site_id, identity.site_id,
                         sizeof(config_.site_id) - 1);
            config_.site_id[sizeof(config_.site_id) - 1] = '\0';
            if (app::ConfigStore{}.save(config_) != ESP_OK) {
                ESP_LOGE(tag, "Could not save installation ID received from master ESP");
                continue;
            }
            ESP_LOGI(tag, "Adopted installation ID %s from master ESP; restarting",
                     config_.site_id);
            vTaskDelay(pdMS_TO_TICKS(250));
            esp_restart();
        }
        last_receive_tick_.store(xTaskGetTickCount());
        dispatch(header.type, payload.data(), header.payload_size);
    }
}

void CoordinatorLink::dispatch(std::uint8_t type, const std::uint8_t *payload,
                               std::uint16_t size)
{
    const bool coordinator = config_.profile.device_role == app::DeviceRole::parallel_coordinator;
    const bool gateway = config_.profile.device_role == app::DeviceRole::internet_gateway;
    if (type == type_telemetry && gateway && size == sizeof(TelemetryMessage)) {
        TelemetryMessage message{};
        std::memcpy(&message, payload, size);
        sink_.on_telemetry(message);
    } else if (type == type_command && coordinator && size == sizeof(RoutedCommand)) {
        RoutedCommand message{};
        std::memcpy(&message, payload, size);
        sink_.on_command(message);
    } else if (type == type_command_result && gateway &&
               size == sizeof(RoutedCommandResult)) {
        RoutedCommandResult message{};
        std::memcpy(&message, payload, size);
        sink_.on_command_result(message);
    }
}

static_assert(sizeof(TelemetryMessage) <= maximum_payload);
static_assert(sizeof(IdentityPayload) <= maximum_payload);
static_assert(sizeof(RoutedCommand) <= maximum_payload);
static_assert(sizeof(RoutedCommandResult) <= maximum_payload);

} // namespace inverter_gateway::network
