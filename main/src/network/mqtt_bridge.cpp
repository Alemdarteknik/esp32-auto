#include "inverter_gateway/network/mqtt_bridge.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/socket.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "inverter_gateway/inverter/inverter_command.hpp"
#include "inverter_gateway/inverter/register_catalog.hpp"
#include "inverter_gateway/inverter/register_value.hpp"

namespace inverter_gateway::network {
namespace {

constexpr char tag[] = "mqtt_bridge";

bool uses_tls(const char *uri)
{
    return uri != nullptr &&
           (std::strncmp(uri, "mqtts://", 8) == 0 ||
            std::strncmp(uri, "wss://", 6) == 0);
}

enum class OutboundType : std::uint8_t { telemetry, command_result };

struct OutboundMessage {
    OutboundType type;
    TelemetryMessage telemetry{};
    RoutedCommandResult command_result{};
};

bool parse_operation(const char *name, inverter::LocalOperation &operation)
{
    if (name == nullptr) return false;
    constexpr inverter::LocalOperation operations[] = {
        inverter::LocalOperation::set_buzzer_enabled,
        inverter::LocalOperation::set_bluetooth_enabled,
        inverter::LocalOperation::set_inverter_enabled,
        inverter::LocalOperation::set_overload_restart_enabled,
        inverter::LocalOperation::set_overload_to_bypass_enabled,
        inverter::LocalOperation::set_battery_type,
    };
    for (const auto candidate : operations) {
        if (std::strcmp(name, inverter::operation_name(candidate)) == 0) {
            operation = candidate;
            return true;
        }
    }
    return false;
}

bool parse_command_id(const cJSON *item, std::uint64_t &command_id)
{
    if (cJSON_IsNumber(item) && item->valuedouble >= 1 &&
        item->valuedouble <= 9007199254740991.0 &&
        item->valuedouble == static_cast<double>(static_cast<std::uint64_t>(item->valuedouble))) {
        command_id = static_cast<std::uint64_t>(item->valuedouble);
        return true;
    }
    if (cJSON_IsString(item)) {
        if (item->valuestring == nullptr || item->valuestring[0] == '\0') return false;
        for (const char *character = item->valuestring; *character != '\0'; ++character) {
            if (*character < '0' || *character > '9') return false;
        }
        errno = 0;
        char *end = nullptr;
        command_id = std::strtoull(item->valuestring, &end, 10);
        return errno != ERANGE && command_id != 0 && end != item->valuestring && *end == '\0';
    }
    return false;
}

bool parse_named_category(const cJSON *item, inverter::RegisterSpace &space)
{
    if (!cJSON_IsString(item)) return false;
    if (std::strcmp(item->valuestring, "configuration") == 0) {
        space = inverter::RegisterSpace::holding;
        return true;
    }
    if (std::strcmp(item->valuestring, "input_data") == 0) {
        space = inverter::RegisterSpace::input;
        return true;
    }
    return false;
}

bool parse_register_domain(const cJSON *item, inverter::RegisterDomain &domain)
{
    if (!cJSON_IsString(item)) return false;
    constexpr inverter::RegisterDomain domains[] = {
        inverter::RegisterDomain::identity, inverter::RegisterDomain::communications,
        inverter::RegisterDomain::system, inverter::RegisterDomain::inverter,
        inverter::RegisterDomain::grid, inverter::RegisterDomain::eps,
        inverter::RegisterDomain::pv, inverter::RegisterDomain::generator,
        inverter::RegisterDomain::battery, inverter::RegisterDomain::bms,
        inverter::RegisterDomain::load, inverter::RegisterDomain::power,
        inverter::RegisterDomain::energy, inverter::RegisterDomain::parallel,
        inverter::RegisterDomain::diagnostics, inverter::RegisterDomain::reserved,
    };
    for (const auto candidate : domains) {
        std::string expected = inverter::register_domain_name(candidate);
        std::transform(expected.begin(), expected.end(), expected.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(character >= 'A' && character <= 'Z'
                                                        ? character - 'A' + 'a'
                                                        : character);
                       });
        if (expected == item->valuestring) {
            domain = candidate;
            return true;
        }
    }
    return false;
}

const inverter::RegisterDescriptor *find_named_register(
    inverter::RegisterSpace space, const char *name, const char *description,
    const inverter::RegisterDomain *domain)
{
    if (name == nullptr || name[0] == '\0') return nullptr;
    const auto catalog = space == inverter::RegisterSpace::holding
                             ? inverter::holding_register_catalog()
                             : inverter::input_register_catalog();
    const inverter::RegisterDescriptor *match = nullptr;
    for (std::size_t index = 0; index < catalog.size; ++index) {
        const auto &candidate = catalog.data[index];
        if (std::strcmp(candidate.name, name) != 0) continue;
        if (domain != nullptr && candidate.domain != *domain) continue;
        if (description != nullptr && description[0] != '\0' &&
            std::strcmp(candidate.description, description) != 0) continue;
        if (match != nullptr) return nullptr;
        match = &candidate;
    }
    return match;
}

bool encode_named_value(
    const inverter::RegisterDescriptor &descriptor, const cJSON *item,
    std::array<std::uint16_t, inverter::register_command_word_count> &destination,
    std::uint16_t &word_count)
{
    if (cJSON_IsBool(item)) {
        if (descriptor.data_type != inverter::RegisterDataType::uint8 &&
            descriptor.data_type != inverter::RegisterDataType::uint16 &&
            descriptor.data_type != inverter::RegisterDataType::unknown) return false;
        destination[0] = cJSON_IsTrue(item) ? 1 : 0;
        word_count = 1;
        return true;
    }
    if (cJSON_IsString(item) &&
        (descriptor.data_type == inverter::RegisterDataType::ascii ||
         descriptor.data_type == inverter::RegisterDataType::ascii_or_uint16)) {
        const std::size_t length = std::strlen(item->valuestring);
        if (length == 0 || length > 2) return false;
        destination[0] = static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(item->valuestring[0]) << 8U);
        if (length == 2) {
            destination[0] |= static_cast<std::uint8_t>(item->valuestring[1]);
        }
        word_count = 1;
        return true;
    }
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        descriptor.scale <= 0.0) return false;

    double encoded = item->valuedouble / descriptor.scale;
    if (descriptor.space == inverter::RegisterSpace::holding &&
        descriptor.address == 21 && item->valuedouble >= 2000.0 &&
        item->valuedouble <= 2099.0) {
        encoded = item->valuedouble - 2000.0;
    }
    const double rounded = std::round(encoded);
    if (std::fabs(encoded - rounded) > 0.000001) return false;
    switch (descriptor.data_type) {
    case inverter::RegisterDataType::sint16:
        if (rounded < -32768.0 || rounded > 32767.0) return false;
        destination[0] = static_cast<std::uint16_t>(
            static_cast<std::int16_t>(rounded));
        word_count = 1;
        return true;
    case inverter::RegisterDataType::uint32:
        if (rounded < 0.0 || rounded > 4294967295.0) return false;
        {
            const auto raw = static_cast<std::uint32_t>(rounded);
            destination[0] = static_cast<std::uint16_t>(raw >> 16U);
            destination[1] = static_cast<std::uint16_t>(raw);
            word_count = 2;
        }
        return true;
    case inverter::RegisterDataType::sint32:
        if (rounded < -2147483648.0 || rounded > 2147483647.0) return false;
        {
            const auto raw = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(rounded));
            destination[0] = static_cast<std::uint16_t>(raw >> 16U);
            destination[1] = static_cast<std::uint16_t>(raw);
            word_count = 2;
        }
        return true;
    case inverter::RegisterDataType::uint8:
        if (rounded < 0.0 || rounded > 255.0) return false;
        destination[0] = static_cast<std::uint16_t>(rounded);
        word_count = 1;
        return true;
    case inverter::RegisterDataType::uint16:
    case inverter::RegisterDataType::ascii_or_uint16:
    case inverter::RegisterDataType::unknown:
        if (rounded < 0.0 || rounded > 65535.0) return false;
        destination[0] = static_cast<std::uint16_t>(rounded);
        word_count = 1;
        return true;
    default:
        return false;
    }
}

void add_register_details(cJSON *root, const char *key,
                          inverter::RegisterSpace space, std::uint16_t first,
                          std::uint16_t count, const std::uint16_t *values)
{
    cJSON *entries = cJSON_AddArrayToObject(root, key);
    for (std::uint16_t index = 0; index < count; ++index) {
        cJSON *entry = cJSON_CreateObject();
        const std::uint16_t address = first + index;
        const auto *descriptor = inverter::find_register(space, address);
        cJSON_AddNumberToObject(entry, "address", address);
        cJSON_AddNumberToObject(entry, "raw", values[index]);
        if (descriptor != nullptr) {
            cJSON_AddStringToObject(entry, "name", descriptor->name);
            cJSON_AddStringToObject(entry, "unit", descriptor->unit);
            const bool has_low = index + 1 < count;
            const auto decoded = inverter::decode_numeric_value(
                *descriptor, values[index], has_low ? values[index + 1] : 0,
                has_low);
            if (decoded.valid) {
                cJSON_AddNumberToObject(entry, "value", decoded.engineering_value);
            }
        }
        cJSON_AddItemToArray(entries, entry);
    }
}

void add_named_value(cJSON *root, const char *key,
                     const inverter::RegisterDescriptor &descriptor,
                     const std::uint16_t *values, std::uint16_t count)
{
    if (descriptor.data_type == inverter::RegisterDataType::ascii) {
        char text_value[3]{};
        text_value[0] = static_cast<char>(values[0] >> 8U);
        text_value[1] = static_cast<char>(values[0] & 0xffU);
        cJSON_AddStringToObject(root, key, text_value);
        return;
    }
    const auto decoded = inverter::decode_numeric_value(
        descriptor, values[0], count > 1 ? values[1] : 0, count > 1);
    if (decoded.valid) cJSON_AddNumberToObject(root, key, decoded.engineering_value);
    else cJSON_AddNullToObject(root, key);
}

std::uint8_t member_from_topic(const char *topic, int length)
{
    const std::string value(topic, static_cast<std::size_t>(length));
    const auto command_position = value.rfind("/command");
    if (command_position == std::string::npos) return 0xff;
    const auto slash = value.rfind('/', command_position - 1);
    if (slash == std::string::npos) return 0xff;
    const std::string id = value.substr(slash + 1, command_position - slash - 1);
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(id.c_str(), &end, 10);
    return end != id.c_str() && *end == '\0' && parsed <= 255
               ? static_cast<std::uint8_t>(parsed)
               : 0xff;
}

} // namespace

MqttBridge::MqttBridge(const app::SystemConfig &config, RuntimeMessageSink &sink)
    : config_(config), sink_(sink)
{
}

esp_err_t MqttBridge::start()
{
    if (client_ != nullptr) return ESP_OK;
    ESP_LOGW(tag, "Starting MQTT connection to %s (authentication %s)",
             config_.mqtt_uri,
             config_.mqtt_username[0] != '\0' ? "enabled" : "disabled");
    publish_queue_ = xQueueCreate(32, sizeof(OutboundMessage));
    if (publish_queue_ == nullptr) return ESP_ERR_NO_MEM;
    esp_mqtt_client_config_t mqtt_config{};
    mqtt_config.broker.address.uri = config_.mqtt_uri;
    if (uses_tls(config_.mqtt_uri)) {
        mqtt_config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        ESP_LOGI(tag, "TLS server verification enabled with the ESP certificate bundle");
    }
    struct ifreq station_interface{};
    esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (station != nullptr &&
        esp_netif_get_netif_impl_name(station, station_interface.ifr_name) ==
            ESP_OK) {
        mqtt_config.network.if_name = &station_interface;
        ESP_LOGW(tag, "MQTT bound to Wi-Fi station interface %s",
                 station_interface.ifr_name);
    } else {
        ESP_LOGW(tag, "Could not bind MQTT explicitly to Wi-Fi station interface");
    }
    mqtt_config.credentials.username = config_.mqtt_username[0] ? config_.mqtt_username : nullptr;
    mqtt_config.credentials.authentication.password =
        config_.mqtt_password[0] ? config_.mqtt_password : nullptr;
    make_topic(availability_topic_, sizeof(availability_topic_),
               config_.profile.logical_member_id, "availability");
    mqtt_config.session.last_will.topic = availability_topic_;
    mqtt_config.session.last_will.msg = "offline";
    mqtt_config.session.last_will.qos = 1;
    mqtt_config.session.last_will.retain = 1;
    mqtt_config.buffer.size = 8192;
    mqtt_config.buffer.out_size = 8192;
    client_ = esp_mqtt_client_init(&mqtt_config);
    if (client_ == nullptr) {
        vQueueDelete(publish_queue_);
        publish_queue_ = nullptr;
        return ESP_FAIL;
    }
    esp_err_t result = esp_mqtt_client_register_event(
        client_, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), event_callback, this);
    if (result == ESP_OK) result = esp_mqtt_client_start(client_);
    if (result == ESP_OK &&
        xTaskCreate(publisher_task, "mqtt_publish", 6144, this, 4,
                    &publisher_task_) != pdPASS) result = ESP_ERR_NO_MEM;
    if (result != ESP_OK) stop();
    return result;
}

void MqttBridge::stop()
{
    if (connected_ && client_ != nullptr) {
        esp_mqtt_client_publish(client_, availability_topic_, "offline", 0, 1, 1);
    }
    connected_ = false;
    if (publisher_task_ != nullptr) {
        vTaskDelete(publisher_task_);
        publisher_task_ = nullptr;
    }
    if (client_ != nullptr) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    if (publish_queue_ != nullptr) {
        vQueueDelete(publish_queue_);
        publish_queue_ = nullptr;
    }
}

void MqttBridge::event_callback(void *argument, esp_event_base_t,
                                std::int32_t, void *event_data)
{
    auto *self = static_cast<MqttBridge *>(argument);
    if (self != nullptr) self->handle_event(static_cast<esp_mqtt_event_handle_t>(event_data));
}

void MqttBridge::handle_event(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED: {
        connected_ = true;
        char topic[192]{};
        std::snprintf(topic, sizeof(topic), "%s/%s/inverter/+/command",
                      config_.mqtt_topic_prefix, config_.site_id);
        esp_mqtt_client_subscribe(client_, topic, 1);
        esp_mqtt_client_publish(client_, availability_topic_, "online", 0, 1, 1);
        ESP_LOGI(tag, "MQTT connected; subscribed to %s", topic);
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        connected_ = false;
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle != nullptr) {
            ESP_LOGE(tag,
                     "MQTT error: type=%d esp_tls=0x%x tls_stack=0x%x socket_errno=%d (%s) broker=%s",
                     static_cast<int>(event->error_handle->error_type),
                     static_cast<unsigned>(event->error_handle->esp_tls_last_esp_err),
                     static_cast<unsigned>(event->error_handle->esp_tls_stack_err),
                     event->error_handle->esp_transport_sock_errno,
                     std::strerror(event->error_handle->esp_transport_sock_errno),
                     config_.mqtt_uri);
        }
        break;
    case MQTT_EVENT_DATA:
        if (event->current_data_offset == 0 && event->data_len == event->total_data_len) {
            handle_command(event->topic, event->topic_len, event->data, event->data_len);
        } else {
            ESP_LOGW(tag, "Rejected fragmented MQTT command");
        }
        break;
    default:
        break;
    }
}

void MqttBridge::handle_command(const char *topic, int topic_length,
                                const char *payload, int payload_length)
{
    if (topic == nullptr || payload == nullptr || payload_length <= 0) return;
    const std::uint8_t target = member_from_topic(topic, topic_length);
    if (target == 0xff || target >= config_.profile.expected_inverter_count) return;
    cJSON *root = cJSON_ParseWithLength(payload, static_cast<std::size_t>(payload_length));
    if (root == nullptr) return;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "command_id");
    const cJSON *operation = cJSON_GetObjectItemCaseSensitive(root, "operation");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    RoutedCommand command{};
    command.target_member_id = target;
    command.origin = RemoteCommandOrigin::mqtt;
    std::uint64_t command_id = 0;
    bool valid = cJSON_IsString(operation) && parse_command_id(id, command_id);
    if (valid && std::strcmp(operation->valuestring, "get") == 0) {
        command.kind = RemoteCommandKind::read_registers;
    } else if (valid && std::strcmp(operation->valuestring, "set") == 0) {
        command.kind = RemoteCommandKind::write_registers;
    } else if (valid) {
        command.kind = RemoteCommandKind::semantic;
        command.commissioning_interlock = cJSON_IsTrue(
            cJSON_GetObjectItemCaseSensitive(root, "commissioning_interlock"));
        command.request.command_id = command_id;
        valid = cJSON_IsNumber(value) &&
                value->valuedouble == static_cast<double>(value->valueint) &&
                parse_operation(operation->valuestring, command.request.operation);
        if (valid) command.request.value = value->valueint;
    }
    if (valid && command.kind != RemoteCommandKind::semantic) {
        constexpr const char *forbidden_public_fields[] = {
            "space", "address", "count", "values", "expected_value",
            "expected_values", "first_register", "register_count", "scale",
        };
        for (const char *field : forbidden_public_fields) {
            if (cJSON_GetObjectItemCaseSensitive(root, field) != nullptr) {
                valid = false;
                break;
            }
        }
        auto &request = command.register_request;
        request.command_id = command_id;
        request.named_request = true;
        const cJSON *category = cJSON_GetObjectItemCaseSensitive(root, "category");
        const inverter::RegisterDescriptor *named_descriptor = nullptr;
        if (command.kind == RemoteCommandKind::write_registers) {
            request.space = inverter::RegisterSpace::holding;
        } else {
            valid = parse_named_category(category, request.space);
        }
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
        const cJSON *description = cJSON_GetObjectItemCaseSensitive(root, "description");
        const cJSON *domain_item = cJSON_GetObjectItemCaseSensitive(root, "domain");
        inverter::RegisterDomain domain{};
        const inverter::RegisterDomain *domain_filter = nullptr;
        if (domain_item != nullptr) {
            valid = valid && parse_register_domain(domain_item, domain);
            if (valid) domain_filter = &domain;
        }
        named_descriptor = valid && cJSON_IsString(name)
                               ? find_named_register(
                                     request.space, name->valuestring,
                                     cJSON_IsString(description)
                                         ? description->valuestring : nullptr,
                                     domain_filter)
                               : nullptr;
        valid = named_descriptor != nullptr &&
                !inverter::descriptor_is_low_word(*named_descriptor);
        if (valid) {
            request.first_register = named_descriptor->address;
            request.register_count = inverter::descriptor_starts_multiword_value(*named_descriptor)
                                         ? 2 : 1;
        }
        if (valid && command.kind == RemoteCommandKind::write_registers) {
            request.confirmed = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "confirmed"));
            request.guarded_interlock = cJSON_IsTrue(
                cJSON_GetObjectItemCaseSensitive(root, "guarded_interlock"));
            request.commissioning_interlock = cJSON_IsTrue(
                cJSON_GetObjectItemCaseSensitive(root, "commissioning_interlock"));
            request.service_interlock = cJSON_IsTrue(
                cJSON_GetObjectItemCaseSensitive(root, "service_interlock"));
            valid = named_descriptor != nullptr &&
                    encode_named_value(*named_descriptor, value, request.values,
                                       request.register_count);
            request.expected_values_present = false;
        }
    }
    if (valid && remember_command_id(command_id)) sink_.on_command(command);
    else if (valid) ESP_LOGW(tag, "Rejected duplicate MQTT command ID %llu",
                            static_cast<unsigned long long>(command_id));
    else ESP_LOGW(tag, "Rejected malformed MQTT command");
    cJSON_Delete(root);
}

bool MqttBridge::remember_command_id(std::uint64_t command_id)
{
    if (std::find(recent_command_ids_.begin(), recent_command_ids_.end(),
                  command_id) != recent_command_ids_.end()) return false;
    recent_command_ids_[next_command_id_slot_] = command_id;
    next_command_id_slot_ = (next_command_id_slot_ + 1U) % recent_command_ids_.size();
    return true;
}

bool is_public_descriptor(const inverter::RegisterDescriptor *descriptor)
{
    return descriptor != nullptr && descriptor->name != nullptr &&
           descriptor->name[0] != '\0' &&
           std::strcmp(descriptor->name, "Reserved") != 0 &&
           !inverter::descriptor_is_low_word(*descriptor);
}

void add_application_value(cJSON *object, const char *key,
                           const inverter::RegisterDescriptor &descriptor,
                           const std::uint16_t *values, std::uint16_t count)
{
    if (descriptor.data_type == inverter::RegisterDataType::ascii ||
        descriptor.data_type == inverter::RegisterDataType::ascii_or_uint16) {
        char text[3]{};
        if (inverter::decode_ascii_word(values[0], text)) {
            cJSON_AddStringToObject(object, key, text);
            return;
        }
    }
    const auto decoded = inverter::decode_numeric_value(
        descriptor, values[0], count > 1 ? values[1] : 0, count > 1);
    if (decoded.valid) cJSON_AddNumberToObject(object, key, decoded.engineering_value);
    else if (descriptor.data_type == inverter::RegisterDataType::ascii_or_uint16) {
        cJSON_AddNumberToObject(object, key, values[0] * descriptor.scale);
    }
    else cJSON_AddNullToObject(object, key);
}

void add_application_details(cJSON *root, const char *key,
                             inverter::RegisterSpace space, std::uint16_t first,
                             std::uint16_t count, const std::uint16_t *values)
{
    cJSON *entries = cJSON_AddArrayToObject(root, key);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto *descriptor = inverter::find_register(space, first + index);
        if (!is_public_descriptor(descriptor)) continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", descriptor->name);
        cJSON_AddStringToObject(entry, "description", descriptor->description);
        cJSON_AddStringToObject(entry, "domain",
                                inverter::register_domain_name(descriptor->domain));
        cJSON_AddStringToObject(entry, "unit", descriptor->unit);
        add_application_value(entry, "value", *descriptor, values + index,
                              static_cast<std::uint16_t>(count - index));
        cJSON_AddItemToArray(entries, entry);
    }
}

void make_topic_component(const char *input, char *output, std::size_t capacity)
{
    if (output == nullptr || capacity == 0) return;
    std::size_t used = 0;
    for (const unsigned char *source =
             reinterpret_cast<const unsigned char *>(input == nullptr ? "" : input);
         *source != 0 && used + 1 < capacity; ++source) {
        const bool alpha_numeric = (*source >= 'a' && *source <= 'z') ||
                                   (*source >= 'A' && *source <= 'Z') ||
                                   (*source >= '0' && *source <= '9');
        if (alpha_numeric) output[used++] = static_cast<char>(*source);
        else if (used != 0 && output[used - 1] != '_') output[used++] = '_';
    }
    while (used != 0 && output[used - 1] == '_') --used;
    if (used == 0 && capacity > 1) {
        output[used++] = 'u';
        if (used + 1 < capacity) output[used++] = 'n';
        if (used + 1 < capacity) output[used++] = 'k';
    }
    output[used] = '\0';
}

bool contains_case_insensitive(const char *text, const char *needle)
{
    if (text == nullptr || needle == nullptr) return false;
    std::string value(text);
    std::string match(needle);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    std::transform(match.begin(), match.end(), match.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value.find(match) != std::string::npos;
}

bool is_alert_descriptor(const inverter::RegisterDescriptor &descriptor)
{
    if (descriptor.read_class != inverter::RegisterReadClass::critical_status) return false;
    return descriptor.address != 1 && descriptor.address != 39;
}

const char *alert_severity(const inverter::RegisterDescriptor &descriptor)
{
    if (contains_case_insensitive(descriptor.name, "warn") ||
        contains_case_insensitive(descriptor.name, "alarm") ||
        contains_case_insensitive(descriptor.description, "warning") ||
        contains_case_insensitive(descriptor.description, "alarm")) return "warning";
    if (contains_case_insensitive(descriptor.name, "error") ||
        contains_case_insensitive(descriptor.name, "fault") ||
        contains_case_insensitive(descriptor.description, "error") ||
        contains_case_insensitive(descriptor.description, "fault")) return "fault";
    if (contains_case_insensitive(descriptor.name, "status")) return "status";
    if (descriptor.address == 0) return "status";
    if (descriptor.address == 23) return "event";
    return "fault";
}

void MqttBridge::publish_telemetry(const TelemetryMessage &message)
{
    if (publish_queue_ == nullptr) return;
    OutboundMessage outbound{};
    outbound.type = OutboundType::telemetry;
    outbound.telemetry = message;
    if (xQueueSend(publish_queue_, &outbound, 0) != pdTRUE) {
        ESP_LOGW(tag, "MQTT publish queue full; telemetry dropped");
    }
}

void MqttBridge::publish_telemetry_now(const TelemetryMessage &message)
{
    if (message.register_space == 0) publish_snapshot(message);
    else {
        publish_live(message);
        publish_alert_changes(message);
    }
}

void MqttBridge::publish_live(const TelemetryMessage &message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "member_id", message.source_member_id);
    cJSON_AddNumberToObject(root, "sequence", message.sequence);
    cJSON_AddNumberToObject(root, "uptime_ms", static_cast<double>(message.timestamp_ms));
    add_application_details(
        root, "data", inverter::RegisterSpace::input, message.first_register,
        std::min<std::uint16_t>(message.register_count,
                               static_cast<std::uint16_t>(message.registers.size())),
        message.registers.data());
    char *json = cJSON_PrintUnformatted(root);
    char topic[192]{};
    make_topic(topic, sizeof(topic), message.source_member_id, "live");
    if (json != nullptr) {
        esp_mqtt_client_publish(client_, topic, json, 0, 0, 0);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

void MqttBridge::publish_snapshot(const TelemetryMessage &message)
{
    const std::uint16_t count = std::min<std::uint16_t>(
        message.register_count,
        static_cast<std::uint16_t>(message.registers.size()));
    const std::uint32_t end = static_cast<std::uint32_t>(message.first_register) + count;
    if (message.first_register <= 5 && end >= 13) {
        char serial_number[17]{};
        std::size_t used = 0;
        for (std::uint16_t address = 5; address <= 12 && used + 1 < sizeof(serial_number);
             ++address) {
            const std::uint16_t word = message.registers[address - message.first_register];
            const unsigned char high = static_cast<unsigned char>(word >> 8U);
            const unsigned char low = static_cast<unsigned char>(word);
            if (std::isprint(high) != 0 && used + 1 < sizeof(serial_number)) {
                serial_number[used++] = static_cast<char>(high);
            }
            if (std::isprint(low) != 0 && used + 1 < sizeof(serial_number)) {
                serial_number[used++] = static_cast<char>(low);
            }
        }
        serial_number[used] = '\0';
        cJSON *identity = cJSON_CreateObject();
        cJSON_AddNumberToObject(identity, "member_id", message.source_member_id);
        cJSON_AddNumberToObject(identity, "sequence", message.sequence);
        cJSON_AddNumberToObject(identity, "uptime_ms",
                                static_cast<double>(message.timestamp_ms));
        cJSON_AddStringToObject(identity, "name", "SerialNumber");
        cJSON_AddStringToObject(identity, "description", "Inverter serial number");
        cJSON_AddStringToObject(identity, "domain", "IDENTITY");
        cJSON_AddStringToObject(identity, "value", serial_number);
        char topic[256]{};
        make_topic(topic, sizeof(topic), message.source_member_id,
                   "snapshot/identity/SerialNumber/Inverter_serial_number");
        char *json = cJSON_PrintUnformatted(identity);
        if (json != nullptr) {
            esp_mqtt_client_publish(client_, topic, json, 0, 1, 1);
            cJSON_free(json);
        }
        cJSON_Delete(identity);
    }
    for (std::uint16_t index = 0; index < count; ++index) {
        const std::uint16_t address = message.first_register + index;
        if (address >= 5 && address <= 12) continue;
        const auto *descriptor = inverter::find_register(
            inverter::RegisterSpace::holding, address);
        if (!is_public_descriptor(descriptor) ||
            descriptor->access == inverter::RegisterAccess::write_only) continue;

        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "member_id", message.source_member_id);
        cJSON_AddNumberToObject(root, "sequence", message.sequence);
        cJSON_AddNumberToObject(root, "uptime_ms",
                                static_cast<double>(message.timestamp_ms));
        cJSON_AddStringToObject(root, "name", descriptor->name);
        cJSON_AddStringToObject(root, "description", descriptor->description);
        cJSON_AddStringToObject(root, "domain",
                                inverter::register_domain_name(descriptor->domain));
        cJSON_AddStringToObject(root, "unit", descriptor->unit);
        add_application_value(root, "value", *descriptor,
                              message.registers.data() + index,
                              static_cast<std::uint16_t>(count - index));

        char domain[32]{};
        char name[64]{};
        char description[64]{};
        make_topic_component(inverter::register_domain_name(descriptor->domain),
                             domain, sizeof(domain));
        make_topic_component(descriptor->name, name, sizeof(name));
        make_topic_component(descriptor->description, description,
                             sizeof(description));
        char suffix[192]{};
        std::snprintf(suffix, sizeof(suffix), "snapshot/%s/%s/%s",
                      domain, name, description);
        char topic[320]{};
        make_topic(topic, sizeof(topic), message.source_member_id, suffix);
        char *json = cJSON_PrintUnformatted(root);
        if (json != nullptr) {
            esp_mqtt_client_publish(client_, topic, json, 0, 1, 1);
            cJSON_free(json);
        }
        cJSON_Delete(root);
    }
}

void MqttBridge::publish_alert_changes(const TelemetryMessage &message)
{
    if (message.source_member_id >= maximum_members) return;
    const std::uint16_t count = std::min<std::uint16_t>(
        message.register_count,
        static_cast<std::uint16_t>(message.registers.size()));
    for (std::uint16_t index = 0; index < count; ++index) {
        const std::uint16_t address = message.first_register + index;
        if (address >= alert_point_count) continue;
        const auto *descriptor = inverter::find_register(
            inverter::RegisterSpace::input, address);
        if (!is_public_descriptor(descriptor) || !is_alert_descriptor(*descriptor)) continue;
        const std::uint16_t value = message.registers[index];
        const bool seen = alert_seen_[message.source_member_id][address];
        const std::uint16_t previous = alert_values_[message.source_member_id][address];
        if (seen && previous == value) continue;
        alert_seen_[message.source_member_id][address] = true;
        alert_values_[message.source_member_id][address] = value;

        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "member_id", message.source_member_id);
        cJSON_AddNumberToObject(root, "sequence", message.sequence);
        cJSON_AddNumberToObject(root, "uptime_ms",
                                static_cast<double>(message.timestamp_ms));
        const char *severity = alert_severity(*descriptor);
        cJSON_AddStringToObject(root, "severity", severity);
        cJSON_AddStringToObject(
            root, "state",
            std::strcmp(severity, "status") == 0 || std::strcmp(severity, "event") == 0
                ? "changed" : (value == 0 ? "clear" : "active"));
        cJSON_AddStringToObject(root, "name", descriptor->name);
        cJSON_AddStringToObject(root, "description", descriptor->description);
        if (seen) cJSON_AddNumberToObject(root, "previous_value", previous);
        else cJSON_AddNullToObject(root, "previous_value");
        cJSON_AddNumberToObject(root, "value", value);

        char name[64]{};
        make_topic_component(descriptor->name, name, sizeof(name));
        char suffix[96]{};
        std::snprintf(suffix, sizeof(suffix), "alert/%s", name);
        char topic[256]{};
        make_topic(topic, sizeof(topic), message.source_member_id, suffix);
        char *json = cJSON_PrintUnformatted(root);
        if (json != nullptr) {
            esp_mqtt_client_publish(client_, topic, json, 0, 1, 1);
            cJSON_free(json);
        }
        cJSON_Delete(root);
    }
}

void MqttBridge::publish_command_result(const RoutedCommandResult &message)
{
    if (publish_queue_ == nullptr) return;
    OutboundMessage outbound{};
    outbound.type = OutboundType::command_result;
    outbound.command_result = message;
    if (xQueueSend(publish_queue_, &outbound, 0) != pdTRUE) {
        ESP_LOGW(tag, "MQTT publish queue full; command result dropped");
    }
}

void MqttBridge::publish_command_result_now(const RoutedCommandResult &message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "member_id", message.source_member_id);
    char command_id[24]{};
    const std::uint64_t id = message.kind == RemoteCommandKind::semantic
                                 ? message.result.command_id
                                 : message.register_result.command_id;
    std::snprintf(command_id, sizeof(command_id), "%llu",
                  static_cast<unsigned long long>(id));
    cJSON_AddStringToObject(root, "command_id", command_id);
    if (message.kind == RemoteCommandKind::semantic) {
        cJSON_AddStringToObject(root, "operation", inverter::operation_name(message.result.operation));
        cJSON_AddStringToObject(root, "outcome", inverter::command_outcome_name(message.result.outcome));
        cJSON_AddBoolToObject(root, "write_sent", message.result.write_was_sent);
    } else {
        const auto &result = message.register_result;
        cJSON_AddStringToObject(root, "operation",
                                result.named_request
                                    ? (message.kind == RemoteCommandKind::read_registers
                                           ? "get" : "set")
                                    : (message.kind == RemoteCommandKind::read_registers
                                           ? "read_registers" : "write_registers"));
        cJSON_AddStringToObject(root, "outcome",
                                inverter::register_command_outcome_name(result.outcome));
        cJSON_AddStringToObject(root, "write_guard",
                                inverter::write_guard_name(result.strongest_guard));
        if (result.named_request) {
            const auto *descriptor = inverter::find_register(
                result.space, result.first_register);
            if (descriptor != nullptr) {
                cJSON_AddStringToObject(root, "name", descriptor->name);
                cJSON_AddStringToObject(root, "description", descriptor->description);
                cJSON_AddStringToObject(root, "domain",
                                        inverter::register_domain_name(descriptor->domain));
                cJSON_AddStringToObject(root, "unit", descriptor->unit);
                if (result.previous_values_valid) {
                    add_named_value(root, "previous_value", *descriptor,
                                    result.previous_values.data(), result.register_count);
                }
                if (result.result_values_valid) {
                    add_named_value(root, "value", *descriptor,
                                    result.result_values.data(), result.register_count);
                }
            }
        } else {
            cJSON_AddStringToObject(root, "space",
                                    result.space == inverter::RegisterSpace::holding
                                        ? "holding" : "input");
            cJSON_AddNumberToObject(root, "first_register", result.first_register);
            cJSON_AddNumberToObject(root, "register_count", result.register_count);
            if (result.previous_values_valid) {
                cJSON *previous = cJSON_AddArrayToObject(root, "previous_values");
                for (std::uint16_t index = 0; index < result.register_count; ++index) {
                    cJSON_AddItemToArray(previous,
                                        cJSON_CreateNumber(result.previous_values[index]));
                }
            }
            if (result.result_values_valid) {
                cJSON *values = cJSON_AddArrayToObject(root, "values");
                for (std::uint16_t index = 0; index < result.register_count; ++index) {
                    cJSON_AddItemToArray(values,
                                        cJSON_CreateNumber(result.result_values[index]));
                }
                add_register_details(root, "registers", result.space,
                                     result.first_register, result.register_count,
                                     result.result_values.data());
            }
        }
        if (!result.modbus_write.ok() &&
            result.modbus_write.status != protocol::ModbusWriteStatus::invalid_argument) {
            cJSON_AddNumberToObject(root, "modbus_status",
                                    static_cast<int>(result.modbus_write.status));
            cJSON_AddNumberToObject(root, "modbus_exception",
                                    result.modbus_write.exception_code);
        }
    }
    char *json = cJSON_PrintUnformatted(root);
    char topic[192]{};
    make_topic(topic, sizeof(topic), message.source_member_id, "result");
    if (json != nullptr) {
        esp_mqtt_client_publish(client_, topic, json, 0, 1, 0);
        cJSON_free(json);
    }
    cJSON_Delete(root);
    publish_setting_snapshot(message);
}

void MqttBridge::publish_setting_snapshot(const RoutedCommandResult &message)
{
    if (message.kind != RemoteCommandKind::write_registers) return;
    const auto &result = message.register_result;
    if (!result.named_request ||
        result.outcome != inverter::RegisterCommandOutcome::confirmed ||
        !result.result_values_valid) return;
    const auto *descriptor = inverter::find_register(result.space, result.first_register);
    if (!is_public_descriptor(descriptor)) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "member_id", message.source_member_id);
    char command_id[24]{};
    std::snprintf(command_id, sizeof(command_id), "%llu",
                  static_cast<unsigned long long>(result.command_id));
    cJSON_AddStringToObject(root, "updated_by_command_id", command_id);
    cJSON_AddStringToObject(root, "name", descriptor->name);
    cJSON_AddStringToObject(root, "description", descriptor->description);
    cJSON_AddStringToObject(root, "domain",
                            inverter::register_domain_name(descriptor->domain));
    cJSON_AddStringToObject(root, "unit", descriptor->unit);
    add_application_value(root, "value", *descriptor,
                          result.result_values.data(), result.register_count);

    char domain[32]{};
    char name[64]{};
    char description[64]{};
    make_topic_component(inverter::register_domain_name(descriptor->domain),
                         domain, sizeof(domain));
    make_topic_component(descriptor->name, name, sizeof(name));
    make_topic_component(descriptor->description, description,
                         sizeof(description));
    char suffix[192]{};
    std::snprintf(suffix, sizeof(suffix), "snapshot/%s/%s/%s",
                  domain, name, description);
    char topic[320]{};
    make_topic(topic, sizeof(topic), message.source_member_id, suffix);
    char *json = cJSON_PrintUnformatted(root);
    if (json != nullptr) {
        esp_mqtt_client_publish(client_, topic, json, 0, 1, 1);
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

void MqttBridge::publisher_task(void *argument)
{
    auto *self = static_cast<MqttBridge *>(argument);
    OutboundMessage message{};
    while (self != nullptr) {
        if (xQueueReceive(self->publish_queue_, &message, portMAX_DELAY) != pdTRUE) continue;
        while (!self->connected_) vTaskDelay(pdMS_TO_TICKS(250));
        if (message.type == OutboundType::telemetry) {
            self->publish_telemetry_now(message.telemetry);
        } else {
            self->publish_command_result_now(message.command_result);
        }
    }
}

void MqttBridge::make_topic(char *destination, std::size_t capacity,
                            std::uint8_t member_id, const char *suffix) const
{
    std::snprintf(destination, capacity, "%s/%s/inverter/%u/%s",
                  config_.mqtt_topic_prefix, config_.site_id, member_id, suffix);
}

} // namespace inverter_gateway::network
