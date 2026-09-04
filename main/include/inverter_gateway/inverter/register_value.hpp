#pragma once

#include <cstdint>

#include "inverter_gateway/inverter/register_catalog.hpp"

namespace inverter_gateway::inverter {

struct DecodedRegisterValue {
    bool valid = false;
    std::uint8_t word_count = 0;
    bool is_signed = false;
    std::uint32_t unsigned_raw = 0;
    std::int32_t signed_raw = 0;
    double engineering_value = 0.0;
};

DecodedRegisterValue decode_numeric_value(const RegisterDescriptor &descriptor,
                                          std::uint16_t high_or_single_word,
                                          std::uint16_t low_word = 0,
                                          bool low_word_available = false);

bool decode_ascii_word(std::uint16_t raw, char output[3]);
bool descriptor_starts_multiword_value(const RegisterDescriptor &descriptor);
bool descriptor_is_low_word(const RegisterDescriptor &descriptor);

} // namespace inverter_gateway::inverter
