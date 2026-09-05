#pragma once

#include <cstddef>
#include <cstdint>

namespace inverter_gateway::inverter {

inline constexpr std::size_t documented_holding_register_count = 265;
inline constexpr std::size_t documented_input_register_count = 422;

enum class RegisterSpace : std::uint8_t {
    holding,
    input,
};

enum class RegisterAccess : std::uint8_t {
    unknown,
    read_only,
    write_only,
    read_write,
};

enum class RegisterDataType : std::uint8_t {
    unknown,
    uint8,
    uint16,
    sint16,
    uint32,
    uint32_low_word,
    sint32,
    sint32_low_word,
    ascii,
    ascii_or_uint16,
};

enum class RegisterReadClass : std::uint8_t {
    boot_identity,
    setup_snapshot,
    critical_status,
    live_flow,
    live_health,
    slow_counter,
    diagnostic,
    reserved,
};

enum class RegisterWriteGuard : std::uint8_t {
    read_only,
    hot_edit_confirmed,
    hot_edit_guarded,
    runtime_command,
    standby_required,
    commissioning_only,
    service_only,
    blocked_unvalidated,
};

enum class RegisterDomain : std::uint8_t {
    identity,
    communications,
    system,
    inverter,
    grid,
    eps,
    pv,
    generator,
    battery,
    bms,
    load,
    power,
    energy,
    parallel,
    diagnostics,
    reserved,
};

enum RegisterFeature : std::uint16_t {
    feature_none = 0,
    feature_three_phase = 1U << 0,
    feature_extended_pv = 1U << 1,
    feature_generator = 1U << 2,
    feature_battery = 1U << 3,
    feature_bms = 1U << 4,
    feature_parallel = 1U << 5,
    feature_service = 1U << 6,
    feature_pv = 1U << 7,
};

struct RegisterDescriptor {
    RegisterSpace space;
    std::uint16_t address;
    const char *name;
    const char *description;
    RegisterAccess access;
    RegisterDataType data_type;
    const char *unit;
    double scale;
    RegisterReadClass read_class;
    RegisterWriteGuard write_guard;
    RegisterDomain domain;
    std::uint16_t feature_flags;
    const char *default_value;
    const char *value_range;
    bool stored;
    const char *notes;
    const char *applicable_models;
};

struct RegisterCatalogView {
    const RegisterDescriptor *data;
    std::size_t size;
};

RegisterCatalogView holding_register_catalog();
RegisterCatalogView input_register_catalog();
const RegisterDescriptor *find_register(RegisterSpace space, std::uint16_t address);

const char *read_class_name(RegisterReadClass value);
const char *write_guard_name(RegisterWriteGuard value);
const char *register_domain_name(RegisterDomain value);

} // namespace inverter_gateway::inverter
