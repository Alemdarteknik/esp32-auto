#pragma once

#include <array>
#include <cstdint>
#include <atomic>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "inverter_gateway/app/system_config.hpp"
#include "inverter_gateway/network/runtime_messages.hpp"

namespace inverter_gateway::network {

class MqttBridge {
public:
    MqttBridge(const app::SystemConfig &config, RuntimeMessageSink &sink);
    esp_err_t start();
    void stop();
    bool connected() const { return connected_.load(); }
    void publish_telemetry(const TelemetryMessage &message);
    void publish_command_result(const RoutedCommandResult &result);

private:
    static void event_callback(void *argument, esp_event_base_t base,
                               std::int32_t event_id, void *event_data);
    void handle_event(esp_mqtt_event_handle_t event);
    void handle_command(const char *topic, int topic_length,
                        const char *payload, int payload_length);
    void make_topic(char *destination, std::size_t capacity,
                    std::uint8_t member_id, const char *suffix) const;
    static void publisher_task(void *argument);
    void publish_telemetry_now(const TelemetryMessage &message);
    void publish_command_result_now(const RoutedCommandResult &result);
    void publish_live(const TelemetryMessage &message);
    void publish_snapshot(const TelemetryMessage &message);
    void publish_alert_changes(const TelemetryMessage &message);
    void publish_setting_snapshot(const RoutedCommandResult &message);
    bool remember_command_id(std::uint64_t command_id);

    const app::SystemConfig &config_;
    RuntimeMessageSink &sink_;
    esp_mqtt_client_handle_t client_ = nullptr;
    std::atomic_bool connected_{false};
    QueueHandle_t publish_queue_ = nullptr;
    TaskHandle_t publisher_task_ = nullptr;
    char availability_topic_[192]{};
    std::array<std::uint64_t, 32> recent_command_ids_{};
    std::size_t next_command_id_slot_ = 0;
    static constexpr std::size_t alert_point_count = 512;
    static constexpr std::size_t maximum_members = app::max_saved_peers + 1;
    std::array<std::array<std::uint16_t, alert_point_count>, maximum_members>
        alert_values_{};
    std::array<std::array<bool, alert_point_count>, maximum_members> alert_seen_{};
};

} // namespace inverter_gateway::network
