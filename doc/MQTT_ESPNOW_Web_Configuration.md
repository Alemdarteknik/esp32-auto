# MQTT, ESP-NOW, and Web Configuration

## Runtime roles

| Role | Local inverter | ESP-NOW | Wi-Fi/MQTT | Purpose |
|---|---:|---:|---:|---|
| `standalone_combined` | Yes | No | Yes | One ESP32 reads one inverter and publishes directly. |
| `parallel_coordinator` | Yes | Coordinator | No | Reads the master, gathers members, and uses UART to a separate gateway. |
| `parallel_member` | Yes | Member | No | Reads one slave inverter and sends its data to the coordinator. |
| `internet_gateway` | No | No | Yes | Bridges the coordinator UART link to MQTT. |
| `coordinator_gateway_combined` | Yes | Coordinator | Yes | Reads the master, gathers members, and publishes MQTT itself. |

Native three-phase and grouped three-phase installations use the topology and phase fields. Each inverter-facing ESP32 also stores its number of physically connected PV inputs. It still talks only to its locally attached inverter, so each member's PV values remain individually identifiable.

## Installation sequence

1. Configure each member ESP32 with a unique member ID from 1 upward, the same topology, system counts, and radio channel. The installation ID is learned automatically from the master.
2. Configure the coordinator with member ID 0 and the exact expected member count.
3. If used, cross-connect coordinator UART1 TX/RX to gateway UART1 RX/TX, connect ground, and configure the gateway's Wi-Fi/MQTT credentials.
4. Leave every device in discovery mode until the web status reports the expected links.
5. Finalize the coordinator only after all members are present. Finalize MQTT roles only after Wi-Fi and MQTT are connected.

Joining the `INVERTER-xxxxxx` setup access point triggers standard captive-portal discovery on supported phones and computers. The setup page remains available directly at `http://192.168.4.1`. Holding BOOT for three seconds during normal operation saves a setup-mode request and restarts the ESP into this access point without deleting its current settings.

The Advanced settings section can enable detailed ESP serial-monitor output for commissioning or troubleshooting. When disabled, routine information messages, Modbus frame dumps, and successful CRC messages are suppressed while warnings and errors remain visible. This preference is stored separately in NVS. Firmware defaults are centralized in `main/include/inverter_gateway/app/project_config.hpp`; ESP-IDF menuconfig can additionally force monitor output on with `CONFIG_INVERTER_GATEWAY_MONITOR_OUTPUT_DEFAULT`.

Peer associations survive restart in NVS. Changing role, topology, member ID, or ESP-NOW channel clears the old associations and starts discovery again.

The **Connected PV inputs** selection accepts 0 through 16. The fast PV1/PV2 block is shortened at its end when fewer than two inputs are configured. The PV3-PV16 block is skipped when it is unnecessary and otherwise shortened to the selected channel count. Individual PV-power polling is also limited to the selected count. Blocks containing both PV and non-PV measurements must still be read, but the MQTT encoder removes the unused PV fields.

The installation page also records whether a battery, BMS communication link, and generator are present. No battery blocks are polled when no battery is installed. With a non-communicating battery, ordinary battery values remain enabled while the BMS block is skipped. Generator blocks are skipped unless a generator is selected, and generator S/T blocks are limited to native three-phase inverters.

Live filtering also follows the selected topology. Single-phase and grouped single-phase nodes omit S/T and other three-phase-only fields. Native three-phase nodes retain R/S/T fields. Standalone systems omit parallel-only fields, while parallel systems retain them. Each routed telemetry message carries this applicability information, so a separate Internet Gateway applies the originating inverter's settings rather than its own.

After commissioning, an MQTT-capable ESP continues to serve the page at `http://esp32.local` and at its LAN IP. The client must be on the same Wi-Fi network. This does not reactivate the setup access point. Saving a changed configuration restarts connection verification before returning to normal operation.

The master ESP creates a device ID from its Wi-Fi station MAC address, for example `INV-7CDFA1123456`. During commissioning, slave ESPs learn and save that ID from the master over ESP-NOW. A separate Internet Gateway learns and saves it from the master through the coordinator cable, then restarts before connecting to MQTT. After finalization, a device will not adopt a different device ID. The advanced web settings retain an optional ID override for restoring an existing installation.

The device ID groups the complete system; the logical member ID distinguishes its inverters. ESP-NOW and UART frames include a device-ID hash to reject traffic belonging to a different finalized installation. The MAC-derived ID is an identifier, not a password or authentication secret.

The coordinator-to-gateway link uses GPIO17/GPIO18 at 115200 8N1. It carries framed binary messages with a version, length, and CRC, and both ends send heartbeats. The web page refuses finalization until this link is live.

## Telemetry envelope

```json
{
  "member_id": 1,
  "sequence": 42,
  "uptime_ms": 123456,
  "space": "input",
  "first_register": 40,
  "register_count": 2,
  "registers": [
    {"address": 40, "raw": 0, "name": "...", "unit": "...", "value": 0},
    {"address": 41, "raw": 0, "name": "...", "unit": "...", "value": 0}
  ]
}
```

Identity and setup blocks are sent once after an inverter connection. Critical, fast, normal, and energy-counter blocks use their polling classes. On-demand diagnostic and service blocks are not automatically polled.

## Command behavior

MQTT is the only remote command source. The web page changes ESP/gateway configuration; it does not write inverter registers.

The topic member ID selects the inverter. A coordinator executes member 0 locally and routes other IDs over ESP-NOW. A separate Internet Gateway relays the same envelope over its framed UART link. Results return through the reverse path to the same MQTT hierarchy.

Two command interfaces are available:

- named semantic operations for common controls;
- catalog-name `get` and `set` operations that the MQTT gateway resolves to Modbus descriptors;
- catalog-backed `read_registers` and `write_registers` operations for explicit raw register access.

Explicit commands are limited to 24 contiguous words so the same envelope fits MQTT, UART and ESP-NOW. Multiword settings must include both high and low words. Every write requires `confirmed`, every readable write target requires `expected_values`, and successful writes are read back. Guarded, commissioning and service classes require their explicit interlock; standby, commissioning and service classes also require a fresh standby/stopped state. Full examples and outcomes are in `MQTT_Register_Command_API.md`.

MQTT QoS 1 can redeliver a command. Semantic operations are absolute. Generic writes compare current raw words to `expected_values`; a stale or repeated request is rejected with `expected_value_mismatch` unless the caller deliberately submits a new command based on the latest value.

## Required production hardening

- Add ESP-NOW PMK/LMK encryption and protected key commissioning.
- Add verified MQTT TLS CA/client-certificate provisioning.
- Add authorization levels for owner, installer, and service operations.
- Hardware-test radio coexistence, reconnect behavior, broker outages, and every permitted write on the exact FSC model/firmware.
