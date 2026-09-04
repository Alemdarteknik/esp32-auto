#pragma once

#include <atomic>
#include <cstdint>

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "inverter_gateway/app/system_config.hpp"
#include "inverter_gateway/network/runtime_messages.hpp"

namespace inverter_gateway::network {

class CoordinatorLink {
public:
    CoordinatorLink(app::SystemConfig &config, RuntimeMessageSink &sink);
    esp_err_t start();
    void stop();
    bool connected() const;
    esp_err_t send_telemetry(const TelemetryMessage &message);
    esp_err_t send_command(const RoutedCommand &command);
    esp_err_t send_command_result(const RoutedCommandResult &result);

private:
    static void worker_task(void *argument);
    void run();
    esp_err_t send(std::uint8_t type, const void *payload, std::uint16_t size);
    esp_err_t enqueue(std::uint8_t type, const void *payload, std::uint16_t size);
    void dispatch(std::uint8_t type, const std::uint8_t *payload, std::uint16_t size);

    app::SystemConfig &config_;
    RuntimeMessageSink &sink_;
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t send_mutex_ = nullptr;
    QueueHandle_t transmit_queue_ = nullptr;
    std::atomic<TickType_t> last_receive_tick_{0};
    bool started_ = false;
};

} // namespace inverter_gateway::network
