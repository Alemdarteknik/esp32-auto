#include <cinttypes>
#include <atomic>
#include <memory>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "inverter_gateway/app/monitor_output.hpp"
#include "inverter_gateway/app/project_config.hpp"
#include "inverter_gateway/app/system_config.hpp"
#include "inverter_gateway/inverter/command_service.hpp"
#include "inverter_gateway/inverter/production_poller.hpp"
#include "inverter_gateway/inverter/register_command_service.hpp"
#include "inverter_gateway/inverter/register_catalog.hpp"
#include "inverter_gateway/network/espnow_mesh.hpp"
#include "inverter_gateway/network/coordinator_link.hpp"
#include "inverter_gateway/network/mqtt_bridge.hpp"
#include "inverter_gateway/network/runtime_messages.hpp"
#include "inverter_gateway/network/wifi_provisioning.hpp"
#include "inverter_gateway/protocol/modbus_rtu.hpp"
#include "inverter_gateway/transport/usb_rs485.hpp"
#include "nvs_flash.h"

#if !CONFIG_IDF_TARGET_ESP32S3
#error "This application requires an ESP32-S3 with USB-OTG host support."
#endif

namespace {

constexpr char tag[] = "gateway";

class UniversalRuntime final : public inverter_gateway::network::RuntimeMessageSink {
public:
    explicit UniversalRuntime(inverter_gateway::app::SystemConfig &config) : config_(config)
    {
        command_queue_ = xQueueCreate(8, sizeof(inverter_gateway::network::RoutedCommand));
    }

    ~UniversalRuntime() override
    {
        if (command_queue_ != nullptr) vQueueDelete(command_queue_);
    }

    void attach_mesh(inverter_gateway::network::EspNowMesh *mesh) { mesh_ = mesh; }
    void attach_mqtt(inverter_gateway::network::MqttBridge *mqtt) { mqtt_ = mqtt; }
    void attach_coordinator_link(inverter_gateway::network::CoordinatorLink *link) { coordinator_link_ = link; }

    void on_telemetry(const inverter_gateway::network::TelemetryMessage &message) override
    {
        if (message.source_member_id == config_.profile.logical_member_id) {
            local_inverter_seen_ = true;
        }
        if (inverter_gateway::app::role_is_espnow_member(config_.profile.device_role)) {
            if (mesh_ != nullptr) mesh_->send_telemetry(message);
        } else if (mqtt_ != nullptr) {
            mqtt_->publish_telemetry(message);
        } else if (inverter_gateway::app::role_is_espnow_coordinator(config_.profile.device_role) &&
                   coordinator_link_ != nullptr) {
            coordinator_link_->send_telemetry(message);
        }
    }

    void on_command(const inverter_gateway::network::RoutedCommand &command) override
    {
        if (command.origin != inverter_gateway::network::RemoteCommandOrigin::mqtt) {
            ESP_LOGW(tag, "Rejected command without MQTT origin");
            return;
        }
        const std::uint8_t local_id = config_.profile.logical_member_id;
        if (inverter_gateway::app::role_has_local_inverter(config_.profile.device_role) &&
            command.target_member_id == local_id && command_queue_ != nullptr) {
            if (xQueueSend(command_queue_, &command, 0) != pdTRUE) {
                const std::uint64_t command_id =
                    command.kind == inverter_gateway::network::RemoteCommandKind::semantic
                        ? command.request.command_id
                        : command.register_request.command_id;
                ESP_LOGW(tag, "Command queue full; rejected command %" PRIu64,
                         command_id);
            }
        } else if (inverter_gateway::app::role_is_espnow_coordinator(config_.profile.device_role) &&
                   mesh_ != nullptr) {
            const esp_err_t result = mesh_->send_command(command);
            if (result != ESP_OK) {
                ESP_LOGW(tag, "Could not route command to member %u: %s",
                         command.target_member_id, esp_err_to_name(result));
            }
        } else if (config_.profile.device_role == inverter_gateway::app::DeviceRole::internet_gateway &&
                   coordinator_link_ != nullptr) {
            coordinator_link_->send_command(command);
        }
    }

    void on_command_result(const inverter_gateway::network::RoutedCommandResult &result) override
    {
        if (inverter_gateway::app::role_is_espnow_member(config_.profile.device_role)) {
            if (mesh_ != nullptr) mesh_->send_command_result(result);
        } else if (mqtt_ != nullptr) {
            mqtt_->publish_command_result(result);
        } else if (inverter_gateway::app::role_is_espnow_coordinator(config_.profile.device_role) &&
                   coordinator_link_ != nullptr) {
            coordinator_link_->send_command_result(result);
        }
    }

    bool receive_command(inverter_gateway::network::RoutedCommand &command)
    {
        return command_queue_ != nullptr &&
               xQueueReceive(command_queue_, &command, 0) == pdTRUE;
    }

    bool local_inverter_seen() const { return local_inverter_seen_; }
    bool telemetry_transport_ready() const
    {
        return !inverter_gateway::app::role_is_espnow_member(config_.profile.device_role) ||
               (mesh_ != nullptr && mesh_->has_coordinator());
    }

private:
    inverter_gateway::app::SystemConfig &config_;
    QueueHandle_t command_queue_ = nullptr;
    inverter_gateway::network::EspNowMesh *mesh_ = nullptr;
    inverter_gateway::network::MqttBridge *mqtt_ = nullptr;
    inverter_gateway::network::CoordinatorLink *coordinator_link_ = nullptr;
    std::atomic_bool local_inverter_seen_{false};
};

std::uint16_t enabled_features(const inverter_gateway::app::SystemConfig &config)
{
    using namespace inverter_gateway::inverter;
    std::uint16_t features = feature_none;
    if (config.profile.connected_pv_inputs > 0) features |= feature_pv;
    if (config.profile.connected_pv_inputs > 2) features |= feature_extended_pv;
    if (config.profile.battery_installed) features |= feature_battery;
    if (config.profile.battery_installed && config.profile.bms_connected) {
        features |= feature_bms;
    }
    if (config.profile.generator_installed) features |= feature_generator;
    if (config.profile.topology == inverter_gateway::app::SystemTopology::standalone_native_three_phase ||
        config.profile.topology == inverter_gateway::app::SystemTopology::parallel_native_three_phase) {
        features |= feature_three_phase;
    }
    if (config.profile.topology == inverter_gateway::app::SystemTopology::parallel_single_phase ||
        config.profile.topology == inverter_gateway::app::SystemTopology::parallel_three_phase_groups ||
        config.profile.topology == inverter_gateway::app::SystemTopology::parallel_native_three_phase) {
        features |= feature_parallel;
    }
    return features;
}

[[noreturn]] void run_local_inverter(inverter_gateway::app::SystemConfig &config,
                                     UniversalRuntime &runtime)
{
    using inverter_gateway::transport::UsbRs485;
    ESP_ERROR_CHECK(UsbRs485::install(inverter_gateway::app::usb_buffer_size));
    while (true) {
        UsbRs485::begin_connection_session();
        std::unique_ptr<CdcAcmDevice> device =
            UsbRs485::open(inverter_gateway::app::usb_open_timeout_ms,
                           inverter_gateway::app::usb_buffer_size);
        if (!device) {
            vTaskDelay(pdMS_TO_TICKS(inverter_gateway::app::usb_reconnect_delay_ms));
            continue;
        }
        if (UsbRs485::configure(*device, inverter_gateway::app::inverter_baud) != ESP_OK) {
            device.reset();
            vTaskDelay(pdMS_TO_TICKS(inverter_gateway::app::usb_configure_retry_ms));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(inverter_gateway::app::usb_settle_time_ms));
        UsbRs485::clear_receive_buffer();
        inverter_gateway::protocol::ModbusRtuClient client(
            *device, config.profile.modbus_slave_address,
            inverter_gateway::app::modbus_response_timeout_ms,
            inverter_gateway::app::modbus_minimum_command_gap_ms);
        inverter_gateway::inverter::ProductionPoller poller(
            client, config.profile.logical_member_id,
            config.profile.connected_pv_inputs,
            static_cast<std::uint8_t>(config.profile.phase), runtime);
        inverter_gateway::inverter::CommandService commands(client);
        inverter_gateway::inverter::RegisterCommandService register_commands(client);

        while (!UsbRs485::disconnected()) {
            inverter_gateway::network::RoutedCommand command{};
            if (runtime.receive_command(command)) {
                auto state = poller.state();
                inverter_gateway::network::RoutedCommandResult routed_result{};
                routed_result.source_member_id = config.profile.logical_member_id;
                routed_result.kind = command.kind;
                if (command.kind == inverter_gateway::network::RemoteCommandKind::semantic) {
                    state.battery_commissioning_interlock = command.commissioning_interlock;
                    routed_result.result = commands.execute(command.request, state);
                } else {
                    routed_result.register_result = register_commands.execute(
                        command.register_request, state,
                        command.kind == inverter_gateway::network::RemoteCommandKind::write_registers);
                }
                runtime.on_command_result(routed_result);
            }
            if (runtime.telemetry_transport_ready()) {
                poller.run_due(enabled_features(config));
            }
            vTaskDelay(pdMS_TO_TICKS(inverter_gateway::app::inverter_loop_delay_ms));
        }
        ESP_LOGW(tag, "USB-RS485 adapter disconnected");
        device.reset();
        vTaskDelay(pdMS_TO_TICKS(inverter_gateway::app::usb_reconnect_delay_ms));
    }
}

[[noreturn]] void idle_forever()
{
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}

bool mqtt_ready(void *context)
{
    auto *mqtt = static_cast<inverter_gateway::network::MqttBridge *>(context);
    return mqtt != nullptr && mqtt->connected();
}

bool wait_for_station_ip(inverter_gateway::network::WifiProvisioning &wifi)
{
    const TickType_t started = xTaskGetTickCount();
    while (!wifi.station_connected() &&
           xTaskGetTickCount() - started <
               pdMS_TO_TICKS(inverter_gateway::app::mqtt_wifi_ready_wait_ms)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return wifi.station_connected();
}

bool inverter_ready(void *context)
{
    auto *runtime = static_cast<UniversalRuntime *>(context);
    return runtime != nullptr && runtime->local_inverter_seen();
}

esp_err_t clear_factory_setup()
{
    const esp_err_t config_result = inverter_gateway::app::ConfigStore{}.clear();
    if (config_result != ESP_OK) return config_result;
    // Clear any station credentials retained by older firmware versions that
    // used the Wi-Fi driver's default flash-backed storage.
    return esp_wifi_restore();
}

bool provisioning_button_held()
{
    gpio_config_t configuration{};
    configuration.pin_bit_mask = 1ULL << inverter_gateway::app::provisioning_button_gpio;
    configuration.mode = GPIO_MODE_INPUT;
    configuration.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&configuration);
    if (gpio_get_level(inverter_gateway::app::provisioning_button_gpio) != 0) return false;
    const TickType_t started = xTaskGetTickCount();
    while (gpio_get_level(inverter_gateway::app::provisioning_button_gpio) == 0) {
        if (xTaskGetTickCount() - started >=
            pdMS_TO_TICKS(inverter_gateway::app::provisioning_button_hold_ms)) return true;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return false;
}

void provisioning_button_task(void *argument)
{
    auto *config = static_cast<inverter_gateway::app::SystemConfig *>(argument);
    bool timing_press = false;
    TickType_t pressed_at = 0;
    while (config != nullptr) {
        if (gpio_get_level(inverter_gateway::app::provisioning_button_gpio) == 0) {
            if (!timing_press) {
                timing_press = true;
                pressed_at = xTaskGetTickCount();
            } else if (xTaskGetTickCount() - pressed_at >=
                       pdMS_TO_TICKS(inverter_gateway::app::provisioning_button_hold_ms)) {
                const esp_err_t result = clear_factory_setup();
                if (result == ESP_OK) {
                    ESP_LOGW(tag,
                             "Factory reset complete; release BOOT to restart in first-installation mode");
                    while (gpio_get_level(
                               inverter_gateway::app::provisioning_button_gpio) == 0) {
                        vTaskDelay(pdMS_TO_TICKS(25));
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                }
                ESP_LOGE(tag, "Factory reset failed: %s",
                         esp_err_to_name(result));
                while (gpio_get_level(inverter_gateway::app::provisioning_button_gpio) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                timing_press = false;
            }
        } else {
            timing_press = false;
        }
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    vTaskDelete(nullptr);
}

} // namespace

extern "C" void app_main(void)
{
    const esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result != ESP_OK) {
        ESP_LOGE(tag, "NVS initialization failed: %s", esp_err_to_name(nvs_result));
        return;
    }
    const esp_err_t monitor_result = inverter_gateway::app::load_monitor_output();
    if (monitor_result != ESP_OK) {
        ESP_LOGW(tag, "Could not load serial-monitor setting: %s",
                 esp_err_to_name(monitor_result));
    }

    inverter_gateway::app::SystemConfig config{};
    config.profile.expected_inverter_count = 1;
    const esp_err_t load_result = inverter_gateway::app::ConfigStore{}.load(config);
    if (config.site_id[0] == '\0') {
        ESP_ERROR_CHECK(inverter_gateway::app::ensure_hardware_site_id(config));
    }
    const auto validation = inverter_gateway::app::validate_config(config);
    const bool configuration_ready = load_result == ESP_OK &&
        validation == inverter_gateway::app::ConfigValidation::valid;
    inverter_gateway::network::WifiProvisioning wifi(config);
    ESP_ERROR_CHECK(wifi.initialize());
    if (configuration_ready && provisioning_button_held()) {
        ESP_ERROR_CHECK(clear_factory_setup());
        ESP_LOGW(tag,
                 "Factory reset complete; release BOOT to restart in first-installation mode");
        while (gpio_get_level(inverter_gateway::app::provisioning_button_gpio) == 0) {
            vTaskDelay(pdMS_TO_TICKS(25));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }
    UniversalRuntime runtime(config);
    wifi.set_inverter_ready_probe(inverter_ready, &runtime);

    // A valid saved configuration may be in either normal operation or the
    // post-save verification portal. Monitor BOOT in both states so a long
    // press always performs the same complete factory reset.
    if (configuration_ready &&
        xTaskCreate(provisioning_button_task, "setup_button", 3072, &config, 5,
                    nullptr) != pdPASS) {
        ESP_LOGE(tag, "Could not start setup-button monitor");
    }

    if (!configuration_ready || !config.provisioned) {
        ESP_LOGW(tag, "Provisioning mode (%s)",
                 configuration_ready ? "awaiting_verification" :
                 inverter_gateway::app::config_validation_name(validation));
        ESP_ERROR_CHECK(wifi.start_portal());
        std::unique_ptr<inverter_gateway::network::EspNowMesh> mesh;
        if (configuration_ready && inverter_gateway::app::role_uses_espnow(config.profile.device_role)) {
            mesh = std::make_unique<inverter_gateway::network::EspNowMesh>(config, runtime);
            runtime.attach_mesh(mesh.get());
            ESP_ERROR_CHECK(mesh->start());
            wifi.set_mesh(mesh.get());
        }
        std::unique_ptr<inverter_gateway::network::CoordinatorLink> coordinator_link;
        if (configuration_ready &&
            inverter_gateway::app::role_uses_coordinator_link(config.profile.device_role)) {
            coordinator_link = std::make_unique<inverter_gateway::network::CoordinatorLink>(config, runtime);
            runtime.attach_coordinator_link(coordinator_link.get());
            wifi.set_coordinator_link(coordinator_link.get());
            ESP_ERROR_CHECK(coordinator_link->start());
        }
        std::unique_ptr<inverter_gateway::network::MqttBridge> mqtt;
        const bool waiting_for_master_identity =
            config.profile.device_role == inverter_gateway::app::DeviceRole::internet_gateway &&
            inverter_gateway::app::site_id_is_hardware_default(config);
        if (configuration_ready && inverter_gateway::app::role_uses_mqtt(config.profile.device_role) &&
            !waiting_for_master_identity) {
            if (!wait_for_station_ip(wifi)) {
                ESP_LOGW(tag,
                         "Wi-Fi has no IP yet; MQTT will start and retry in the background");
            }
            mqtt = std::make_unique<inverter_gateway::network::MqttBridge>(config, runtime);
            const esp_err_t mqtt_start_result = mqtt->start();
            if (mqtt_start_result == ESP_OK) {
                runtime.attach_mqtt(mqtt.get());
                wifi.set_mqtt_ready_probe(mqtt_ready, mqtt.get());
            } else {
                ESP_LOGE(tag, "MQTT could not start: %s; setup AP remains active",
                         esp_err_to_name(mqtt_start_result));
                mqtt.reset();
            }
        }
        if (configuration_ready &&
            inverter_gateway::app::role_has_local_inverter(config.profile.device_role)) {
            run_local_inverter(config, runtime);
        }
        idle_forever();
    }

    ESP_LOGI(tag, "Starting role=%s topology=%s",
             inverter_gateway::app::device_role_name(config.profile.device_role),
             inverter_gateway::app::topology_name(config.profile.topology));
    if (inverter_gateway::app::role_uses_mqtt(config.profile.device_role)) {
        ESP_ERROR_CHECK(wifi.start_station());
    } else {
        ESP_ERROR_CHECK(wifi.start_radio(config.profile.espnow_channel));
    }
    std::unique_ptr<inverter_gateway::network::EspNowMesh> mesh;
    if (inverter_gateway::app::role_uses_espnow(config.profile.device_role)) {
        mesh = std::make_unique<inverter_gateway::network::EspNowMesh>(config, runtime);
        runtime.attach_mesh(mesh.get());
        ESP_ERROR_CHECK(mesh->start());
        wifi.set_mesh(mesh.get());
    }

    std::unique_ptr<inverter_gateway::network::CoordinatorLink> coordinator_link;
    if (inverter_gateway::app::role_uses_coordinator_link(config.profile.device_role)) {
        coordinator_link = std::make_unique<inverter_gateway::network::CoordinatorLink>(config, runtime);
        runtime.attach_coordinator_link(coordinator_link.get());
        wifi.set_coordinator_link(coordinator_link.get());
        ESP_ERROR_CHECK(coordinator_link->start());
    }

    std::unique_ptr<inverter_gateway::network::MqttBridge> mqtt;
    if (inverter_gateway::app::role_uses_mqtt(config.profile.device_role)) {
        if (!wait_for_station_ip(wifi)) {
            ESP_LOGW(tag,
                     "Wi-Fi has no IP yet; MQTT will start and retry in the background");
        }
        mqtt = std::make_unique<inverter_gateway::network::MqttBridge>(config, runtime);
        const esp_err_t mqtt_start_result = mqtt->start();
        if (mqtt_start_result == ESP_OK) {
            runtime.attach_mqtt(mqtt.get());
            wifi.set_mqtt_ready_probe(mqtt_ready, mqtt.get());
        } else {
            ESP_LOGE(tag, "MQTT could not start: %s",
                     esp_err_to_name(mqtt_start_result));
            mqtt.reset();
        }
    }

    if (inverter_gateway::app::role_uses_mqtt(config.profile.device_role)) {
        const esp_err_t webpage_result = wifi.start_lan_page();
        if (webpage_result != ESP_OK) {
            ESP_LOGE(tag, "Configuration webpage could not start: %s",
                     esp_err_to_name(webpage_result));
        }
    }

    if (inverter_gateway::app::role_has_local_inverter(config.profile.device_role)) {
        run_local_inverter(config, runtime);
    }
    idle_forever();
}
