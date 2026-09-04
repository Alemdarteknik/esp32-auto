#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "inverter_gateway/inverter/inverter_command.hpp"
#include "inverter_gateway/inverter/polling_plan.hpp"
#include "inverter_gateway/network/runtime_messages.hpp"
#include "inverter_gateway/protocol/modbus_rtu.hpp"

namespace inverter_gateway::inverter {

class ProductionPoller {
public:
    ProductionPoller(protocol::ModbusRtuClient &client, std::uint8_t member_id,
                     network::RuntimeMessageSink &sink);
    void reset();
    void run_due(std::uint16_t enabled_features);
    InverterStateSnapshot state() const;

private:
    bool due(std::size_t index, const PollRequest &request, std::int64_t now_ms) const;
    void read_request(std::size_t index, const PollRequest &request,
                      std::int64_t now_ms);
    void emit(const PollRequest &request, const std::uint16_t *values,
              std::uint16_t count, std::int64_t timestamp_ms);
    void update_state(const PollRequest &request, const std::uint16_t *values,
                      std::uint16_t count);

    static constexpr std::size_t maximum_requests = 32;
    protocol::ModbusRtuClient &client_;
    std::uint8_t member_id_;
    network::RuntimeMessageSink &sink_;
    std::array<std::int64_t, maximum_requests> last_poll_ms_{};
    std::array<bool, maximum_requests> completed_once_{};
    InverterStateSnapshot state_{};
    std::int64_t state_updated_ms_ = 0;
    std::uint32_t sequence_ = 0;
};

} // namespace inverter_gateway::inverter
