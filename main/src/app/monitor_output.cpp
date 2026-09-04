#include "inverter_gateway/app/monitor_output.hpp"

#include <atomic>
#include <cstdint>

#include "esp_log.h"
#include "inverter_gateway/app/project_config.hpp"
#include "nvs.h"
#include "sdkconfig.h"

namespace inverter_gateway::app {
namespace {

constexpr char nvs_namespace[] = "inv_gateway";
constexpr char nvs_key[] = "monitor_output";
#if defined(CONFIG_INVERTER_GATEWAY_MONITOR_OUTPUT_DEFAULT) && CONFIG_INVERTER_GATEWAY_MONITOR_OUTPUT_DEFAULT
constexpr bool default_enabled = true;
#else
constexpr bool default_enabled = serial_monitor_output_default;
#endif
std::atomic_bool enabled_{default_enabled};

void apply(bool enabled)
{
    enabled_.store(enabled);
    esp_log_level_set("*", enabled ? ESP_LOG_INFO : ESP_LOG_WARN);
}

} // namespace

bool monitor_output_enabled()
{
    return enabled_.load();
}

esp_err_t load_monitor_output()
{
    nvs_handle_t handle = 0;
    const esp_err_t open_result = nvs_open(nvs_namespace, NVS_READONLY, &handle);
    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        apply(default_enabled);
        return ESP_OK;
    }
    if (open_result != ESP_OK) return open_result;
    std::uint8_t stored = 0;
    const esp_err_t result = nvs_get_u8(handle, nvs_key, &stored);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        apply(default_enabled);
        return ESP_OK;
    }
    if (result == ESP_OK) apply(stored != 0);
    return result;
}

esp_err_t save_monitor_output(bool enabled)
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_u8(handle, nvs_key, enabled ? 1 : 0);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result == ESP_OK) apply(enabled);
    return result;
}

} // namespace inverter_gateway::app
