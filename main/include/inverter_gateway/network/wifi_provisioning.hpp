#pragma once

#include <atomic>

#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/task.h"
#include "inverter_gateway/app/system_config.hpp"

namespace inverter_gateway::network {

class EspNowMesh;
class CoordinatorLink;

class WifiProvisioning {
public:
    using ReadyProbe = bool (*)(void *context);
    explicit WifiProvisioning(app::SystemConfig &config);
    esp_err_t initialize();
    esp_err_t start_station();
    esp_err_t start_radio(std::uint8_t channel);
    esp_err_t start_portal();
    esp_err_t start_lan_page();
    void set_mqtt_ready_probe(ReadyProbe probe, void *context);
    void set_inverter_ready_probe(ReadyProbe probe, void *context);
    void set_mesh(EspNowMesh *mesh);
    void set_coordinator_link(CoordinatorLink *link);
    bool station_connected() const { return station_connected_.load(); }

private:
    static void wifi_event(void *argument, esp_event_base_t base,
                           std::int32_t event_id, void *event_data);
    static esp_err_t page_handler(httpd_req_t *request);
    static esp_err_t config_get_handler(httpd_req_t *request);
    static esp_err_t config_post_handler(httpd_req_t *request);
    static esp_err_t wifi_scan_handler(httpd_req_t *request);
    static esp_err_t finalize_handler(httpd_req_t *request);
    static esp_err_t status_handler(httpd_req_t *request);
    static esp_err_t captive_redirect_handler(httpd_req_t *request);
    static void captive_dns_task(void *argument);
    static void verification_task(void *argument);
    static void reboot_task(void *argument);
    esp_err_t start_http_server();
    esp_err_t receive_json(httpd_req_t *request, char *buffer, std::size_t capacity);
    bool required_connections_ready() const;
    esp_err_t complete_provisioning();

    app::SystemConfig &config_;
    httpd_handle_t server_ = nullptr;
    bool initialized_ = false;
    std::atomic_bool station_connected_{false};
    std::atomic_bool scan_active_{false};
    std::atomic_bool suppress_station_reconnect_{false};
    std::atomic_bool completion_started_{false};
    ReadyProbe mqtt_ready_probe_ = nullptr;
    void *mqtt_ready_context_ = nullptr;
    ReadyProbe inverter_ready_probe_ = nullptr;
    void *inverter_ready_context_ = nullptr;
    EspNowMesh *mesh_ = nullptr;
    CoordinatorLink *coordinator_link_ = nullptr;
    TaskHandle_t captive_dns_task_ = nullptr;
    TaskHandle_t verification_task_ = nullptr;
    static WifiProvisioning *instance_;
};

} // namespace inverter_gateway::network
