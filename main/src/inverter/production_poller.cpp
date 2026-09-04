#include "inverter_gateway/inverter/production_poller.hpp"

#include <algorithm>

#include "esp_log.h"
#include "esp_timer.h"
#include "inverter_gateway/app/project_config.hpp"
#include "inverter_gateway/inverter/register_value.hpp"

namespace inverter_gateway::inverter {
namespace {

constexpr char tag[] = "production_poll";

std::int64_t cadence_interval_ms(PollCadence cadence)
{
    switch (cadence) {
    case PollCadence::critical: return app::critical_poll_interval_ms;
    case PollCadence::fast: return app::fast_poll_interval_ms;
    case PollCadence::normal: return app::normal_poll_interval_ms;
    case PollCadence::slow: return app::slow_poll_interval_ms;
    default: return 0;
    }
}

InverterOperatingState decode_state(std::uint16_t raw)
{
    switch (raw) {
    case 0: return InverterOperatingState::stopped;
    case 1:
    case 2:
    case 5:
    case 6:
    case 7: return InverterOperatingState::operating;
    case 3: return InverterOperatingState::faulted;
    case 4: return InverterOperatingState::updating;
    default: return InverterOperatingState::unknown;
    }
}

} // namespace

ProductionPoller::ProductionPoller(protocol::ModbusRtuClient &client,
                                   std::uint8_t member_id,
                                   network::RuntimeMessageSink &sink)
    : client_(client), member_id_(member_id), sink_(sink)
{
    reset();
}

void ProductionPoller::reset()
{
    last_poll_ms_.fill(0);
    completed_once_.fill(false);
    state_ = {};
    state_updated_ms_ = 0;
    sequence_ = 0;
}

InverterStateSnapshot ProductionPoller::state() const
{
    auto result = state_;
    result.fresh = state_updated_ms_ != 0 &&
                   esp_timer_get_time() / 1000 - state_updated_ms_ <=
                       app::inverter_state_freshness_ms;
    return result;
}

void ProductionPoller::run_due(std::uint16_t enabled_features)
{
    const auto plan = default_poll_plan();
    const std::int64_t now_ms = esp_timer_get_time() / 1000;
    for (std::size_t index = 0; index < plan.size && index < maximum_requests; ++index) {
        const auto &request = plan.data[index];
        if (poll_request_is_enabled(request, enabled_features) && due(index, request, now_ms)) {
            read_request(index, request, now_ms);
            return;
        }
    }
}

bool ProductionPoller::due(std::size_t index, const PollRequest &request,
                           std::int64_t now_ms) const
{
    if (request.cadence == PollCadence::on_demand) return false;
    if (request.cadence == PollCadence::boot_once || request.cadence == PollCadence::setup_once) {
        return !completed_once_[index];
    }
    return last_poll_ms_[index] == 0 ||
           now_ms - last_poll_ms_[index] >= cadence_interval_ms(request.cadence);
}

void ProductionPoller::read_request(std::size_t index, const PollRequest &request,
                                    std::int64_t)
{
    std::array<std::uint16_t, 125> values{};
    if (request.register_count > values.size()) return;
    const bool success = request.space == RegisterSpace::holding
                             ? client_.read_holding(request.first_register, request.register_count,
                                                    values.data(), values.size())
                             : client_.read_input(request.first_register, request.register_count,
                                                  values.data(), values.size());
    const std::int64_t completed_at_ms = esp_timer_get_time() / 1000;
    last_poll_ms_[index] = completed_at_ms;
    if (!success) {
        ESP_LOGW(tag, "%s poll failed (%u:%u+%u)", request.name,
                 static_cast<unsigned>(request.space), request.first_register,
                 request.register_count);
        return;
    }
    completed_once_[index] = true;
    update_state(request, values.data(), request.register_count);
    emit(request, values.data(), request.register_count, completed_at_ms);
}

void ProductionPoller::emit(const PollRequest &request, const std::uint16_t *values,
                            std::uint16_t count, std::int64_t timestamp_ms)
{
    std::uint16_t offset = 0;
    while (offset < count) {
        network::TelemetryMessage message{};
        message.source_member_id = member_id_;
        message.register_space = request.space == RegisterSpace::holding ? 0 : 1;
        message.first_register = request.first_register + offset;
        message.register_count = std::min<std::uint16_t>(
            count - offset, static_cast<std::uint16_t>(message.registers.size()));
        if (offset + message.register_count < count && message.register_count > 1) {
            const auto *last = find_register(request.space,
                                             message.first_register + message.register_count - 1);
            if (last != nullptr && descriptor_starts_multiword_value(*last)) {
                --message.register_count;
            }
        }
        message.sequence = ++sequence_;
        message.timestamp_ms = timestamp_ms;
        std::copy_n(values + offset, message.register_count, message.registers.begin());
        sink_.on_telemetry(message);
        offset += message.register_count;
    }
}

void ProductionPoller::update_state(const PollRequest &request,
                                    const std::uint16_t *values,
                                    std::uint16_t count)
{
    if (request.space != RegisterSpace::input || request.first_register != 0 || count == 0) return;
    state_.raw_status = values[0];
    state_.state = decode_state(state_.raw_status);
    state_.fresh = true;
    state_updated_ms_ = esp_timer_get_time() / 1000;
}

} // namespace inverter_gateway::inverter
