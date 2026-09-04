# AOHAI Modbus RTU V2.14 - Application Register Organization

This document organizes the AOHAI register map for an inverter application that must show setup and identity, continuously display live power-system data, detect faults and external changes, safely change permitted settings, and confirm every requested change.

The complete row definitions, scaling, units, enums and model notes remain in `AOHAI_Modbus_RTU_V2.14_Register_Map.md`.

## Implementation status

The ESP-IDF source now implements the universal NVS profile, English AP/web provisioning, class-based production polling, saved ESP-NOW member discovery, the framed coordinator-to-Internet-Gateway UART link, MQTT telemetry, targeted MQTT-only command routing, the guarded semantic write service, catalog-name `get`/`set` resolution at MQTT ingress, and catalog-backed on-demand reads and guarded FC06/FC10 writes. Finalization verifies applicable inverter, member, UART, Wi-Fi, and MQTT links.

The web page configures only the ESP/gateway. It has no inverter-write endpoint. External inverter commands enter through MQTT, then use the same command envelope locally or across UART and ESP-NOW to the selected member.

The security requirements later in this document remain production-hardening work: ESP-NOW PMK/LMK encryption, verified MQTT TLS certificates, user authorization levels, and command-expiry/replay storage are not implemented yet. Until those are completed and hardware-tested, use the networking layer for commissioning/testing rather than unattended production control.

## Safety boundary

`R/W` in the protocol workbook means that Modbus permits a write. It does **not** mean that the setting is electrically safe to change while the inverter is on-grid, charging, discharging or supplying an EPS load.

The FSC user manual explicitly says that parallel mode is available only in standby. It exposes buzzer and Bluetooth as ordinary settings, but it does not provide a global operating-state rule for every Modbus register. This application policy is therefore deliberately conservative.

Never infer write safety from a successful Modbus response. The inverter can accept and store a value that causes a later transfer, shutdown, protection trip or incompatible battery-charging behavior.

## Two independent classifications

A register needs two classifications:

1. **Read class**: why and how often the application reads it.
2. **Write guard**: whether and under which inverter state the application may change it.

For example, H207 belongs to the setup snapshot when read and to hot-edit settings when written. R0 is critical status and is never writable.

Each software descriptor should contain at least:

```text
address, function, name, data_type, signedness, scale, unit,
read_class, write_guard, allowed_values, model_filter,
verification_registers, freshness_limit
```

## Read classes

| Read class | Purpose | Typical policy |
|---|---|---|
| `BOOT_IDENTITY` | Fixed device identity and capabilities | Once after connection/reconnection |
| `SETUP_SNAPSHOT` | Current user and installer settings | After identity, after a write and on manual refresh |
| `CRITICAL_STATUS` | Operating state, protection, faults and warnings | Highest-priority polling and change detection |
| `LIVE_FLOW` | Grid, PV, battery, load, EPS and power flow | Frequent dashboard polling |
| `LIVE_HEALTH` | Temperatures, insulation, leakage, fan and limits | Normal periodic polling |
| `SLOW_COUNTER` | Runtime, cycles, SOH and accumulated energy | Slow periodic polling |
| `CONDITIONAL` | Generator, extra PV channels, BMS and parallel data | Only when the capability is present |
| `DIAGNOSTIC` | DSP/BMS debug information | Manual diagnostics only |
| `RESERVED` | Reserved, blank or undocumented locations | Never interpret or write |

## Write guards

| Write guard | Application behavior |
|---|---|
| `READ_ONLY` | Never write from the application |
| `HOT_EDIT_CONFIRMED` | May be requested while operating; serialize, write once and verify |
| `HOT_EDIT_GUARDED` | Potential runtime setting; enable only after FSC-specific validation and prerequisite checks |
| `RUNTIME_COMMAND` | Event-driven control such as on/off; may arrive at any time and requires transition monitoring |
| `STANDBY_REQUIRED` | Reject while R0 indicates active conversion/output; require verified standby/stopped state |
| `COMMISSIONING_ONLY` | Installer-only, with correct system state and explicit confirmation |
| `SERVICE_ONLY` | Firmware, reset, self-test or fault-clear operation; exclude from the normal settings UI |
| `BLOCKED_UNVALIDATED` | Mapping or safe operating state is insufficiently proven for this FSC model |

`HOT_EDIT_CONFIRMED` means a user may request the change while operating. It never means periodic writing.

## Application state used by write guards

Derive a conservative state from fresh R0/R1, fault registers, currents and powers:

| State | Minimum interpretation |
|---|---|
| `UNKNOWN` | Status is stale, communication is lost or R0 is not decoded |
| `OPERATING` | On-grid, charging, discharging, bypassing or supplying EPS/load |
| `STANDBY` | Valid standby state with no active conversion |
| `STOPPED` | Off/stopped with inactive charge/discharge/output power |
| `FAULTED` | A fault state, code or bitmask is active |
| `UPDATING` | Firmware update is active |

Do not allow standby, commissioning or service writes when state is `UNKNOWN`. Zero power alone is not proof of electrical isolation.

# System topology profiles

Read class and write guard are not enough by themselves. Every connected device also belongs to a topology profile that controls which phases, power pairs and aggregate registers are meaningful.

| Topology profile | Meaning |
|---|---|
| `STANDALONE_1P` | One single-phase inverter operating independently |
| `STANDALONE_3P` | One native three-phase inverter operating independently |
| `PARALLEL_1P` | Two or more single-phase inverters paralleled on the same phase |
| `PARALLEL_3P` | Multiple inverter units assigned to phase groups 3P1/3P2/3P3 |
| `UNKNOWN_TOPOLOGY` | Identity/configuration is incomplete or contradictory; use read-only discovery |

## Topology detection

Determine topology from several consistent sources rather than from one zero-valued measurement:

- device/model identity at H2, H59-H64 and R36;
- parallel mode H211: `0` SIG, `1` PAL, `2` 3P1, `3` 3P2, `4` 3P3;
- parallel CAN address H212 and host/slave type H213;
- parallel identity/status at R450-R455;
- model-rated phase count and FSC manual information;
- validated presence of phase measurements.

Do not classify an inverter as single-phase merely because S/T registers currently contain zero. An idle, disconnected or unsupported three-phase field can also read zero.

H212 is the parallel CAN/device address; H3 is the Modbus slave address. They are different address spaces and must never be substituted for each other.

## One universal firmware image

All ESPs use the same firmware binary. Provisioned configuration in NVS selects the device personality and starts only the necessary services. There is no separate master, slave, three-phase or MQTT firmware build.

| Device personality | Local inverter | ESP-NOW | UART link | Wi-Fi/MQTT |
|---|---:|---|---|---:|
| `STANDALONE_COMBINED` | Yes | Off | Optional/off | Yes |
| `PARALLEL_COORDINATOR` | Yes, normally host/master inverter | Coordinator | To Internet Gateway | No |
| `PARALLEL_MEMBER` | Yes, member/slave inverter | Node | Off | No |
| `INTERNET_GATEWAY` | No | Off | To Parallel Coordinator | Yes |
| `COORDINATOR_GATEWAY_COMBINED` | Yes | Coordinator | Optional/off | Yes; advanced mode with shared-radio channel constraints |

The recommended parallel design uses separate `PARALLEL_COORDINATOR` and `INTERNET_GATEWAY` ESPs. This lets the ESP-NOW network use a fixed channel independently of the building router's Wi-Fi channel.

At boot the universal firmware behaves as follows:

```text
no valid configuration -> start provisioning AP

STANDALONE_COMBINED -> Modbus + Wi-Fi + MQTT
PARALLEL_COORDINATOR -> local Modbus + ESP-NOW coordinator + UART
PARALLEL_MEMBER -> local Modbus + ESP-NOW node
INTERNET_GATEWAY -> UART + Wi-Fi + MQTT
COORDINATOR_GATEWAY_COMBINED -> Modbus + ESP-NOW + Wi-Fi + MQTT
```

## Scenario matrix

| Installation | ESP arrangement |
|---|---|
| One standalone single-phase inverter | One `STANDALONE_COMBINED` ESP |
| One native three-phase inverter | One `STANDALONE_COMBINED` ESP; one Modbus connection reads all three phases |
| Parallel single-phase system | One coordinator at the host inverter, one member ESP per slave inverter, and normally one Internet Gateway |
| Three-phase parallel made from single-phase units | Same coordinator/member arrangement; assign members to 3P1/L1, 3P2/L2 and 3P3/L3 |
| Parallel native three-phase inverters | One inverter ESP per physical inverter plus the coordinator/gateway roles; enable only after model-specific validation |

Do not ask for a P1/P2/P3 assignment for a standalone native three-phase inverter. It exposes all phases from one device. Phase assignments apply to a parallel system built from separately assigned inverter units.

## User/technician provisioning wizard

An authorized user or technician may configure the ESP system. Describing an existing electrical topology is separate from writing topology registers into an inverter.

The wizard should ask questions in this order:

### 1. What is this ESP connected to?

```text
An inverter
Another ESP through UART, for internet access only
```

### 2. What responsibility does this ESP have?

If connected to an inverter:

```text
Standalone inverter controller with MQTT
Parallel-system coordinator
Parallel-system member
Combined parallel coordinator and MQTT gateway
```

If connected only through UART:

```text
Internet/MQTT gateway
```

### 3. What is the system topology?

Configure this once at the coordinator/gateway, not independently on every member:

| Selection | Values/examples |
|---|---|
| Electrical topology | Standalone or parallel |
| Phase topology | Single-phase, native three-phase, or parallel phase groups |
| Total inverter count | Includes the host/master inverter |
| Expected member count | Total inverter count minus the host inverter |
| Parallel phase plan | PAL or assignments to 3P1/3P2/3P3 |

### 4. Which inverter is attached to this inverter ESP?

| Selection | Values/examples |
|---|---|
| Logical member ID | Unique application node ID |
| Inverter role | Host/master or member/slave |
| Phase assignment | None, PAL, L1/3P1, L2/3P2 or L3/3P3 |
| Local Modbus address | H3 address used on the local RS-485 interface |
| Detected identity | Model, serial and firmware read from the inverter |

The firmware should read H211-H213 where supported, propose detected role/phase values and ask for confirmation. The normal wizard must not silently rewrite those registers.

### 5. Configure communications

| Personality | Required configuration |
|---|---|
| `STANDALONE_COMBINED` | Wi-Fi, MQTT and local inverter verification |
| `PARALLEL_COORDINATOR` | Fixed ESP-NOW channel, expected members and UART Internet Gateway link |
| `PARALLEL_MEMBER` | Secure pairing with coordinator and local inverter verification |
| `INTERNET_GATEWAY` | Wi-Fi, MQTT and UART coordinator link |
| `COORDINATOR_GATEWAY_COMBINED` | Wi-Fi/MQTT plus compatible ESP-NOW/router channel policy |

## Centralized member pairing and assignment

The user should configure the plant once rather than repeating the full topology on every node:

1. Configure topology and expected inverter/member counts on the coordinator/gateway.
2. Open a temporary authenticated pairing window.
3. Put each member ESP into pairing mode.
4. An unpaired member scans channels and sends a discovery message.
5. The coordinator authenticates it and reads its reported ESP MAC and local inverter identity.
6. The setup UI assigns its logical member number, inverter role and phase.
7. Coordinator and member save each other's MAC, fixed channel, IDs and encryption information in NVS.
8. After restart, both restore saved peers; routine discovery is not repeated.

Example discovery entry:

```text
ESP MAC: 7C:DF:A1:12:34:56
Inverter model: FSC-12K1P-BL-G3
Inverter serial: <reported serial>
Assignment: Member 2, slave inverter, phase 3P2/L2
```

Pairing mode must be time-limited and user-authorized. A missing saved peer must not allow an unknown device to replace it automatically.

## First-installation validation

For a host inverter plus two slave inverters, validation expects three inverter ESPs plus the Internet Gateway:

```text
Internet Gateway ESP
        | UART
Parallel Coordinator ESP -> host/master inverter
        | ESP-NOW
        +-- Member ESP 1 -> slave inverter 1
        +-- Member ESP 2 -> slave inverter 2
```

Before committing configuration and leaving AP mode, verify:

- the coordinator's local inverter responds and provides valid identity/telemetry;
- the UART Internet Gateway has a bidirectional, version-compatible heartbeat;
- Wi-Fi, DNS/TLS and MQTT authentication work on the Internet Gateway;
- the exact expected number of unique member ESPs is paired;
- every member has valid encrypted ESP-NOW communication;
- every member can read its local inverter identity and at least one valid telemetry sample;
- node IDs, inverter identities, Modbus addresses and phase assignments have no forbidden duplicates;
- exactly one system coordinator and one MQTT gateway are active;
- harmless node telemetry crosses ESP-NOW, UART and MQTT end to end.

Use a diagnostic ping or telemetry sample for reverse-path testing; do not change an inverter setting merely to validate installation.

If verification fails, keep the provisioning AP active and show each unresolved item. If it succeeds, atomically commit configuration and leave AP mode. Later loss of Wi-Fi, MQTT or a node is an operational fault and must not automatically reopen an unsecured AP.

## Operational communication paths

Telemetry:

```text
member inverter -> Modbus -> member ESP -> ESP-NOW
-> coordinator ESP -> UART -> Internet Gateway -> MQTT
```

Commands:

```text
MQTT -> Internet Gateway -> UART -> coordinator
-> local Modbus or targeted ESP-NOW member
-> local write guard + Modbus write/read-back
-> result returns through ESP-NOW/UART/MQTT
```

The Internet Gateway sends structured operations, never unrestricted raw Modbus frames. The coordinator validates system authorization and routing; the target inverter ESP independently enforces its local write guard and current inverter state.

ESP-NOW telemetry/commands and the coordinator-to-gateway UART protocol must carry protocol version, message type, source, target, sequence number, command ID, payload length, payload and integrity check. Duplicate, expired or incorrectly targeted commands must not execute.

## Standalone single-phase behavior

- Present one AC/grid phase, normally the R-phase/L1 fields.
- Poll R2/R5, R42/R43 and R55/R56 for the principal inverter, grid and EPS voltage/current values.
- Use total power pairs where available; do not manufacture L2/L3 values from zeros.
- Poll PV1/PV2 for the FSC-12K and keep them separate.
- Hide S/T phase cards and parallel-system pages.
- Skip R450-R498 unless parallel mode is later enabled and validated.

## Standalone three-phase behavior

- Present R/S/T or L1/L2/L3 separately plus system totals.
- Poll R2-R7, R42-R50 and R55-R60 for all three phases.
- Poll total and per-phase power pairs R284-R307 and R314-R329.
- Check phase imbalance, missing phase and inconsistent line/phase voltage without replacing inverter protection logic.
- Hide parallel aggregate pages while H211 indicates standalone operation.

## Single-phase parallel behavior

- Treat each physical inverter as a device and the parallel group as a separate logical system.
- Keep each device's identity, communication health, faults, temperatures and local measurements separate.
- Use R450-R498 for group totals only after those fields are validated on the host/controller unit.
- Do not add individual values to an already aggregated R450-R498 value, which would double-count power or energy.
- Show both group status and the member that raised a fault.

## Three-phase parallel behavior

- Model the group as three phase collections: 3P1, 3P2 and 3P3.
- Associate every member with its configured phase and host/slave role.
- Show per-phase totals, complete-system totals and member-level health.
- Detect missing members, duplicate parallel addresses, inconsistent phase assignments and inconsistent host/slave configuration.
- Consider the group unavailable for coordinated configuration while any required member is unreachable.

## Device inventory model

The application should separate physical devices from the logical plant:

```text
Plant
  topology: standalone/parallel, one-phase/three-phase
  expected inverter/member count
  coordinator ESP identity
  Internet Gateway ESP identity and UART-link state
  members[]
    ESP identity, MAC and provisioned personality
    ESP-NOW peer/link state
    Modbus slave address (H3)
    parallel address (H212, if used)
    parallel role and phase (H211/H213)
    identity and capabilities
    latest telemetry and freshness
    active faults/warnings
  aggregate power/energy
```

In the preferred design, every inverter ESP has one local RS-485 connection and one local Modbus request queue. If a different installation shares one RS-485 bus between several inverters, every inverter needs a unique validated Modbus address and only one ESP may act as bus master. Never place several ESP Modbus masters on the same electrical RS-485 bus. Do not use Modbus broadcast for control/configuration because it provides no per-device confirmation.

## Writes in a parallel system

| Write type | Parallel-system policy |
|---|---|
| Local preference such as buzzer/Bluetooth | Target the selected physical unit and verify only that unit |
| Plant runtime command such as on/off | Do not assume host-only or all-member behavior; keep disabled until the FSC parallel-control rule is proven |
| Shared operating policy | Require all necessary members reachable; validate compatibility before changing anything |
| Parallel mode/address/role H211-H213 | Require the complete system in standby and installer authorization |
| Battery/BMS topology | Require standby and validate whether the battery is common or per-inverter |
| Grid-code/protection setting | Commissioning-only; values must be consistent across every affected unit |

A coordinated multi-device change is not one atomic Modbus transaction. Track a result for every member. If only some devices accept the change, report `PARTIAL_CONFIGURATION`, block system start if the mismatch is safety-relevant, and require installer resolution. Do not silently report group success and do not automatically roll back unless that exact rollback procedure has been validated.

The FSC manual's parallel commissioning procedure requires configuration in standby and indicates a settling period before the parallel fault clears. The application must monitor member faults/status after configuration instead of treating register read-back alone as completion.

# Read organization

## `BOOT_IDENTITY`

| Registers | Application data |
|---|---|
| H2 | Device type/model identifier |
| H5-H19 | Product serial number |
| H28-H40 | Attestation, ARM and DSP software versions |
| H48-H49 | Hardware versions |
| H50-H57 | Manufacturer information |
| H58 | Modbus protocol version |
| H59-H62 | Inverter model fields |
| H63-H64 | Rated power, 32-bit pair |
| R36 | Runtime device-type consistency check |

Treat writable identity/manufacturer cells as application `READ_ONLY`; they are factory data, not user settings.

## `SETUP_SNAPSHOT`

Read supported holding settings once after identity. Cache raw and decoded values with timestamps. Do not continuously scan all holding registers.

| Registers | Setup page |
|---|---|
| H3-H4, H20-H27 | Communications and clock |
| H68-H95 | Display, safety, grid support, PV/MPPT, CT and anti-backflow |
| H100-H124 | Startup, grid connection, EPS/bypass and protection |
| H125-H147 | Battery topology, limits, SOC and charging controls |
| H150-H182 | Schedules and energy/charging priorities |
| H183-H223 | Battery thresholds/equalization, features, buzzer, bypass, parallel, BMS and generator |
| H231 | Bluetooth |
| H250-H280 | Grid-code and protection thresholds/times |

H67 and H234-H235 are status fields, not ordinary setup values.

## `CRITICAL_STATUS`

| Registers | Meaning | Change event |
|---|---|---|
| R0-R1 | Operating state and grid-connect countdown | State changed |
| R23 | Historical-event count | New historical record may exist |
| R24-R31 | System fault words 0-7 | Fault bit raised/cleared |
| R32-R35 | Main/sub fault and warning codes | Fault/warning raised, changed or cleared |
| R38-R39 | Derating and leading/lagging flags | Limitation changed |
| R130 | BMS state | BMS connection/state changed |
| R131-R134 | BMS error words | BMS error raised/cleared |
| R135-R138 | BMS warning words | BMS warning raised/cleared |
| R167 | Faulty battery/pack address | Fault source changed |
| H67 | Firmware update progress/fault | Poll only during update |
| H234-H235 | Collector network/inverter communication | Connectivity changed |

Modbus RTU does not normally push events. The ESP32 generates events by comparing CRC-valid responses with cached values.

## `LIVE_FLOW`: inverter, grid and EPS

| Registers | Dashboard data |
|---|---|
| R2-R7 | Inverter phase voltages and signed currents |
| R8-R9, R40-R41 | DC-bus and positive/negative bus voltages |
| R42-R53 | Grid voltage/current/frequency, PF and output percentage |
| R54-R60 | EPS frequency, phase voltage and current |
| R284-R307 | Total/per-phase apparent, active and reactive output power |
| R314-R321 | AC power delivered to user/load |
| R322-R329 | AC power delivered to grid |
| R330-R343 | Local-load and EPS power |
| R344 | EPS/off-grid load percentage |
| R345-R356 | System, self-use, battery and extra-grid power |

## `LIVE_FLOW`: PV

| Registers | Data | FSC-12K rule |
|---|---|---|
| R63 | PV path count | Capability hint |
| R64-R67 | PV1/PV2 voltage and current | Poll; keep channels separate |
| R68-R95 | PV3-PV16 voltage/current | `CONDITIONAL`; normally skip on this two-MPPT model |
| R250-R251 | Total PV power | Poll as one 32-bit pair |
| R252-R255 | PV1/PV2 power | Poll as separate pairs |
| R256-R283 | PV3-PV16 power | `CONDITIONAL` |

## Battery/BMS

| Read class | Registers | Data |
|---|---|---|
| `LIVE_FLOW` | R125-R129 | Priority/type, battery voltage, SOC and DSP voltage |
| `CRITICAL_STATUS` | R130-R138 | BMS state/errors/warnings |
| `LIVE_FLOW` | R141-R144 | Current, temperature and BMS current limits |
| `BOOT_IDENTITY` after BMS connection | R145, R147-R149, R170-R171 | Capacity, versions, manufacturer and pack ID |
| `LIVE_HEALTH` | R146, R150, R153-R169 | Remaining capacity, cells, pack and extrema |
| `SLOW_COUNTER` | R151-R152, R172-R175 | Cycles, SOH and cumulative energy |

Disconnected, stale, zero or implausible BMS data must be marked unavailable. Never infer a valid battery solely from a voltage-looking register.

## `LIVE_HEALTH`

| Registers | Data |
|---|---|
| R10-R14 | Inverter, boost/IPM, LLC, battery and ambient temperatures |
| R15-R20 | DC voltage/current components |
| R21-R22 | Insulation resistance and leakage current |
| R106 | Fan speed |
| R107-R108 | Total runtime; slow cadence is sufficient |

## `CONDITIONAL`: generator

| Registers | Data |
|---|---|
| R96-R102 | Generator voltage/current/frequency |
| R357-R368 | Generator apparent/active power |

Poll only if the generator port is configured and validated.

## `SLOW_COUNTER`: energy

| Registers | Data |
|---|---|
| R375-R378 | AC generation today/total |
| R379-R382 | PV energy today/total |
| R383-R390 | Battery charge/discharge energy today/total |
| R391-R398 | AC charge and external grid-connected energy |
| R399-R406 | System generation and self-consumption energy |
| R407-R418 | Load, grid-export and grid-import energy |
| R419-R430 | Sold, purchased and self-supplied energy |

## `CONDITIONAL`: parallel system

| Read class | Registers | Data |
|---|---|---|
| `BOOT_IDENTITY` | R450-R455 | Parallel topology, host serial and device ID |
| `LIVE_FLOW` | R456-R465, R490 | Parallel power and battery SOC |
| `SLOW_COUNTER` | R466-R489, R491-R498 | Parallel energy counters |

Do not poll unless parallel operation is configured and validated.

## `DIAGNOSTIC`, reserved and undocumented

| Registers | Policy |
|---|---|
| R109-R124 | DSP debug; diagnostics only |
| R186-R195 | BMS debug; diagnostics only |
| H41-H47, H96-H99, H105, H111, H117-H120 | Reserved |
| H133-H136, H142-H143, H148-H149, H184, H186 | Reserved/blank |
| H224-H228 | Undefined rows |
| H229-H230, H236-H249 | Undocumented gaps |
| R37, R61-R62, R103-R105, R139-R140, R176-R185, R249, R308-R313 | Reserved |
| R196 | Blank/undefined |
| R197-R248, R369-R374, R431-R449 | Undocumented gaps |

# Write organization

## `HOT_EDIT_CONFIRMED`

These may be requested while operating. They still use the single Modbus queue and require read-back.

| Register | Setting | Write | Verification |
|---|---|---|---|
| H207 | Buzzer | `0` off, `1` on | Read H207; audible behavior is secondary evidence |
| H231 | Bluetooth | `0` off, `1` on | Read H231; warn that disabling it can disconnect a Bluetooth app |

H207 was confirmed on the FSC by a live toggle. H231 is documented in both workbook and FSC manual; retain model validation before production use.

## `HOT_EDIT_GUARDED`

These are useful operating-policy controls, but the workbook does not prove every one is safely applied on-grid. Start with them disabled in the write allowlist and approve individually after FSC testing or manufacturer confirmation.

| Registers | Settings | Required behavior |
|---|---|---|
| H21-H27 | Clock | Write a coherent group; H26 applies time according to the workbook |
| H144-H146 | Generator charge, AC charge and load-priority controls | Check source/battery availability and limits |
| H150-H182 | Schedules and priorities | Validate all periods; write/read a coherent FC10 group where possible |
| H201 bits 0-2 | ECO, overload restart and over-temperature restart | Fresh read-modify-write; preserve unrelated bits |
| H208 | Overload-to-bypass enable | Warn that it changes behavior during a future overload |
| H215 | Grid power compensation | Installer permission and validated range |
| H219-H223 | Generator/smart-load thresholds | Validate SOC/voltage ordering and battery type |

Until individually approved in the FSC allowlist, treat these as `STANDBY_REQUIRED`.

## `RUNTIME_COMMAND`

Runtime commands may arrive at any time. They receive high queue priority but never interrupt a frame or bypass the 850 ms command gap.

| Register | Command | Verification |
|---|---|---|
| H0=`0` | Request inverter off | Read H0, then monitor R0 until stopped/standby or fault |
| H0=`1` | Request inverter on | Read H0, then monitor R0/R1 until operating or fault |
| H147 | Remote energy-priority command | Read H147 and monitor battery/grid/load power; block until enum behavior is FSC-validated |

H0=`0xA5` is marked `TBD` as restart and remains service-only/blocked.

If R0 changes without a matching active transaction, report `EXTERNAL_STATE_CHANGE`, not command success. The cause may be the panel, mobile app, protection logic or another controller.

## `STANDBY_REQUIRED`

These change power routing, output characteristics or equipment relationships. Confirm standby using fresh status and power data before writing.

| Registers | Settings | Reason |
|---|---|---|
| H106-H110 | EPS voltage/frequency and EPS/bypass/UPS modes | Changes output and transfer behavior |
| H125-H141 | Battery chemistry/topology and charge/discharge limits | Wrong values can damage a battery or trip protection |
| H183, H185, H187-H199, H202-H206 | Battery current, thresholds and equalization configuration | Direct charging/discharging behavior; reserved H184/H186 and action H200 are excluded |
| H209-H210 | AC input mode and equalization voltage | Source acceptance/battery charging |
| H211-H214 | Parallel mode/address/type and BMS mode | Manual explicitly requires standby for parallel mode |
| H216-H218 | Generator-port mode/current/rated power | Changes port function and source limits |

For H125 battery type:

```text
fresh communication -> verified standby/stopped -> no charge/discharge
-> validate chemistry and companion BMS settings -> write H125 once
-> read H125 -> refresh R126 and R130-R169 -> report result
```

Never automatically select battery chemistry/protocol. Physical isolation and commissioning must follow inverter/battery manuals and qualified-person procedures.

## `COMMISSIONING_ONLY`

| Registers | Settings | Additional guard |
|---|---|---|
| H3-H4 | Modbus address/baud | Controlled migration with fallback to old communication settings |
| H20 | RS-485 role/mode | Can remove the inverter from slave communication |
| H68 | Screen hardware type | Hardware configuration, not user preference |
| H69-H95 | Safety, grid support, MPPT, CT and anti-backflow | Installer access, grid code and hardware validation |
| H100-H104 | Startup and grid-connect boundaries | Standby plus installer access |
| H112 | Zero-earth relay configuration | Model support/service documentation required |
| H121-H124 | Fault reconnection limits | Grid-code protection values |
| H250-H280 | Safety and grid protection thresholds/times/slopes | Installer authorization and regulatory validation |

Grid export, anti-backflow, CT ratio and grid protection are not convenience settings. Put them in a protected commissioning page with audit logging.

## `SERVICE_ONLY`

| Registers | Operation |
|---|---|
| H5-H19, H57, H59-H62 | Writable factory identity/model fields; normal application reads only |
| H65 | Restore user/factory data |
| H66-H67 | Firmware update workflow |
| H113-H116 | Fan/AFCI test, threshold and fault clear |
| H200 | Start equalization immediately |
| H232 | Data-update notification |
| H233 | Data-logger factory reset/restart-related command |

No automatic retry is allowed for service actions. They require a dedicated UI, confirmation and operation-specific completion check.

## `BLOCKED_UNVALIDATED`

- H1 lacks sufficient meaning and FSC validation.
- H50-H56, H67 and H234-H235 are application read-only even where a generic workbook cell may show broader access.
- Reserved, blank and undocumented registers are always blocked.
- Rows marked unsupported, temporary, `TBC` or `TBD` for single-phase models remain blocked.
- Generic rows whose live values contradict documented ranges remain blocked.
- Any setting without a known safe state defaults to `STANDBY_REQUIRED`, never hot-edit.

# Write-result transaction

`WRITE_RESULT` is an application transaction, not a Modbus address class.

| State | Meaning |
|---|---|
| `QUEUED` | Waiting behind current transaction/gap |
| `SENT` | Transmitted; valid response not yet accepted |
| `WRITE_ACCEPTED` | Valid write response; verification pending |
| `VERIFYING` | Waiting at least 850 ms and reading back |
| `CONFIRMED` | Setting read-back equals request |
| `TRANSITION_PENDING` | Command accepted; related state has not reached target |
| `COMPLETED` | Related live status confirms completion |
| `ADJUSTED` | Inverter stored a different valid value |
| `REJECTED` | Modbus exception or read-back mismatch |
| `TIMEOUT` | Write/verification received no valid response |
| `UNKNOWN` | Write may have occurred; communication was lost before confirmation |

Every result must show register/name, old/requested/read-back values, decoded unit/enum, timestamps, exception, related status and final state.

Standard setting flow:

```text
authorize -> validate model/range/dependencies -> check guard/state
-> queue -> write once -> validate response -> wait >= 850 ms
-> read same H register/group -> compare -> refresh related R data
-> show final result
```

H201 and other bitfields require a fresh read-modify-write-read. Never replace the complete bitmask with one bit or use a stale mask.

Runtime-command flow:

```text
queue -> write once -> read back command register
-> TRANSITION_PENDING -> poll critical status
-> COMPLETED or resulting fault/timeout
```

Do not repeatedly issue the command while waiting for a physical transition.

# Poll scheduler

The protocol allows at most 125 registers per read and documents a minimum 850 ms command interval. Use exactly one transaction worker.

```text
critical: R0-R35
other A : R40-R67                 grid/EPS/PV1/PV2
critical: R0-R35
other B : R125-R175               battery/BMS
critical: R0-R35
other C : R250-R356               powers/load
critical: R0-R35
other D : rotating health/energy/optional block
repeat
```

This checks core status/faults about every 1.7 seconds on a healthy bus. Slow and optional blocks rotate through noncritical slots. An authorized write takes the next permitted slot, performs verification, then critical polling resumes.

| Data | Freshness target |
|---|---:|
| R0-R35 state/faults | About 1.7-2 seconds |
| Grid/EPS/PV/battery/power dashboard | About 5-10 seconds |
| Temperatures/fan/generator | About 10-30 seconds |
| Daily energy | About 30-60 seconds |
| Lifetime totals | About 5-15 minutes |
| Setup snapshot | Startup, manual refresh and after writes |

# Suggested application pages

| Page | Data/actions |
|---|---|
| Dashboard | R0, grid, PV1/PV2, battery, load, EPS and principal powers |
| Energy flow | R250-R356 with signed direction-aware power flow |
| Battery/BMS | R125-R175, validity, limits, cells, SOH and faults |
| Grid/EPS/Generator | R42-R60 and conditional generator data |
| Alarms | R23-R35 and R130-R138 with raised/cleared history |
| Energy | R375-R430 and conditional parallel counters |
| Device information | Boot identity and versions |
| Operating controls | H0 on/off and approved runtime commands |
| Runtime preferences | H207 buzzer, H231 Bluetooth and approved hot settings |
| Energy policy/schedules | Guarded priorities, charging and schedules |
| Battery setup | Standby-required chemistry, BMS and protection values |
| Installer setup | Communications, CT, generator, EPS and parallel settings |
| Grid-code setup | Protected commissioning-only safety parameters |
| Service | Update/reset/test actions, hidden from ordinary users |

# Coverage

This organization accounts for all numeric workbook rows:

- 265 holding rows through H280; H229-H230 and H236-H249 are explicit gaps.
- 422 input rows through R498; R197-R248, R369-R374 and R431-R449 are explicit gaps.

When the workbook, FSC manual and observed behavior disagree, use the safest class and keep writing disabled until FSC-specific behavior is proven.
