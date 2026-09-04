#pragma once

#include "esp_err.h"

namespace inverter_gateway::app {

bool monitor_output_enabled();
esp_err_t load_monitor_output();
esp_err_t save_monitor_output(bool enabled);

} // namespace inverter_gateway::app
