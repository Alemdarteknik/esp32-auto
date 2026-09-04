#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "usb/vcp.hpp"

namespace inverter_gateway::transport {

class UsbRs485 {
public:
    static esp_err_t install(std::size_t buffer_size);
    static std::unique_ptr<CdcAcmDevice> open(std::uint32_t connection_timeout_ms,
                                              std::size_t buffer_size);
    static esp_err_t configure(CdcAcmDevice &device, std::uint32_t baud);

    static void begin_connection_session();
    static bool disconnected();
    static std::size_t clear_receive_buffer();
    static StreamBufferHandle_t receive_stream();
};

} // namespace inverter_gateway::transport
