#include "inverter_gateway/inverter/register_value.hpp"

#include <cctype>

namespace inverter_gateway::inverter {

bool descriptor_starts_multiword_value(const RegisterDescriptor &descriptor)
{
    return descriptor.data_type == RegisterDataType::uint32 ||
           descriptor.data_type == RegisterDataType::sint32;
}

bool descriptor_is_low_word(const RegisterDescriptor &descriptor)
{
    return descriptor.data_type == RegisterDataType::uint32_low_word ||
           descriptor.data_type == RegisterDataType::sint32_low_word;
}

DecodedRegisterValue decode_numeric_value(const RegisterDescriptor &descriptor,
                                          std::uint16_t high_or_single_word,
                                          std::uint16_t low_word,
                                          bool low_word_available)
{
    DecodedRegisterValue result{};
    switch (descriptor.data_type) {
    case RegisterDataType::uint8:
        result.valid = true;
        result.word_count = 1;
        result.unsigned_raw = high_or_single_word & 0x00FFU;
        result.signed_raw = static_cast<std::int32_t>(result.unsigned_raw);
        result.engineering_value = result.unsigned_raw * descriptor.scale;
        break;
    case RegisterDataType::uint16:
        result.valid = true;
        result.word_count = 1;
        result.unsigned_raw = high_or_single_word;
        result.signed_raw = static_cast<std::int32_t>(result.unsigned_raw);
        result.engineering_value = result.unsigned_raw * descriptor.scale;
        break;
    case RegisterDataType::sint16:
        result.valid = true;
        result.word_count = 1;
        result.is_signed = true;
        result.signed_raw = static_cast<std::int16_t>(high_or_single_word);
        result.unsigned_raw = static_cast<std::uint16_t>(high_or_single_word);
        result.engineering_value = result.signed_raw * descriptor.scale;
        break;
    case RegisterDataType::uint32:
        if (!low_word_available) break;
        result.valid = true;
        result.word_count = 2;
        result.unsigned_raw = (static_cast<std::uint32_t>(high_or_single_word) << 16U) |
                              low_word;
        result.signed_raw = static_cast<std::int32_t>(result.unsigned_raw);
        result.engineering_value = result.unsigned_raw * descriptor.scale;
        break;
    case RegisterDataType::sint32:
        if (!low_word_available) break;
        result.valid = true;
        result.word_count = 2;
        result.is_signed = true;
        result.unsigned_raw = (static_cast<std::uint32_t>(high_or_single_word) << 16U) |
                              low_word;
        result.signed_raw = static_cast<std::int32_t>(result.unsigned_raw);
        result.engineering_value = result.signed_raw * descriptor.scale;
        break;
    default:
        break;
    }
    return result;
}

bool decode_ascii_word(std::uint16_t raw, char output[3])
{
    if (output == nullptr) return false;
    const unsigned char high = static_cast<unsigned char>(raw >> 8U);
    const unsigned char low = static_cast<unsigned char>(raw);
    output[0] = std::isprint(high) != 0 ? static_cast<char>(high) : '\0';
    output[1] = std::isprint(low) != 0 ? static_cast<char>(low) : '\0';
    output[2] = '\0';
    return output[0] != '\0' || output[1] != '\0';
}

} // namespace inverter_gateway::inverter
