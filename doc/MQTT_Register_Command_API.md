# MQTT Streams, Getters and Setters - Complete Mosquitto CLI Reference

This document lists every possible MQTT getter that reads data from the inverter and every permitted MQTT setter that writes a setting or command to the inverter. Each executable catalog row has its own `mosquitto_pub` command. The safety classes remain defined in `AOHAI_Modbus_RTU_V2.14_Register_Classes.md`.

## Document map

1. MQTT topic tree, subscriptions, retention and polling timing.
2. Terminus/Linux and Windows CLI quick-start.
3. Getter and setter payloads, lifecycle and returned results.
4. Complete configuration getter catalog.
5. Complete input-data getter catalog.
6. Complete executable setter catalog.
7. Choice-specific commands for every documented multi-option setter.
8. Workbook-writable rows that firmware intentionally blocks.

The internal catalog accounts for all 687 documented rows: 265 configuration items and 422 input-data items. The public API exposes 223 uniquely named configuration getters, 288 uniquely named input-data getters, and 201 named executable setters. For 51 setters with documented discrete choices, the generated reference includes 150 choice-specific Mosquitto commands. Low-word, reserved, unnamed, and ambiguous catalog rows are handled internally or omitted because they do not have a safe public name. Three workbook-writable items remain deliberately non-executable under the current safety policy.

## Public device identity

The public MQTT API does not use a user-created site ID. Each installation is addressed by a stable `device-id` generated automatically from the coordinator or gateway ESP32 hardware MAC address. This prevents installers from accidentally assigning the same identity to different installations.

For a standalone inverter, use member `0`. For a parallel installation, the same hardware-derived `device-id` groups the installation and `member-id` selects the individual inverter: coordinator/master `0`, then members `1...N`.

The server never needs a Modbus register address. Getters and setters use the documented English `name`; Modbus function codes, address spaces, register numbers, raw words, scaling and read-back addresses are private firmware details. Any MQTT payload attempting to provide those internal fields is rejected.

## Command origin and routing

MQTT is the only external inverter-command source. The provisioning web page changes ESP, topology, peer, Wi-Fi and MQTT configuration only; it cannot read or write inverter registers on demand.

The MQTT bridge accepts catalog access only through the named `get` and `set` operations. Raw address-based read/write operations are rejected, as are named commands containing address, space, count, raw-word, scaling, or expected-register fields. It marks every accepted command with an origin field; the coordinator, UART and ESP-NOW links preserve that field, and every inverter node rejects a command without MQTT origin. This is a software routing boundary, not cryptographic proof; transport encryption/authentication is still required for production.

```text
MQTT -> gateway/coordinator -> optional UART -> optional ESP-NOW
     -> selected inverter ESP -> one serialized Modbus worker
     -> result returns to MQTT over the reverse route
```

Command topic:

```text
<prefix>/<device-id>/inverter/<member-id>/command
```

Published topics:

```text
<prefix>/<device-id>/inverter/<member-id>/live
<prefix>/<device-id>/inverter/<member-id>/alert/<alert-name>
<prefix>/<device-id>/inverter/<member-id>/result
<prefix>/<device-id>/inverter/<member-id>/snapshot/<domain>/<name>/<description>
<prefix>/<device-id>/inverter/<member-id>/availability
```

`member-id` is `0` for the standalone/coordinator inverter and `1...N` for parallel members.
In a parallel installation, the Internet-facing ESP publishes every inverter under its own `member-id`; a `+` member wildcard therefore receives the master and every configured slave without opening additional MQTT connections.

| Stream | Purpose | Publish policy |
|---|---|---|
| `live` | Repeated voltage, current, power, PV, battery, load, temperature, status and energy samples | QoS 0, not retained |
| `alert/#` | Inverter faults, warnings, state transitions, derating and BMS faults/warnings | QoS 1, retained per named alert; published only when its value changes |
| `result` | Correlated response to a named getter or setter | QoS 1, not retained |
| `snapshot/#` | Serial/model/firmware and current configuration | QoS 1, retained per named item; refreshed after a confirmed setter |
| `availability` | Device `online`/`offline` state | QoS 1, retained |

`mosquitto_pub` sends a getter or setter. `mosquitto_sub` receives the asynchronous streams and the correlated result. A retained snapshot or alert-state message is delivered immediately when a new subscriber connects.

### What is published where

| Data category | MQTT destination | When it is published |
|---|---|---|
| Grid, inverter, PV, battery, BMS, load, generator and power measurements | `live` | Every successful scheduled poll |
| Energy counters and slower health values | `live` | Every successful normal/slow poll |
| Inverter operating status, fault words, main/sub fault codes, warning codes and derating state | `alert/#` | Initial retained state and then only when changed |
| BMS status, error words, warning words and protected-pack identity | `alert/#` | Initial retained state and then only when changed |
| Serial number, device identity, firmware, protocol version and configuration | `snapshot/#` | Once after inverter communication starts; retained by broker |
| A named getter response | `result` | Once for the matching `command_id` |
| A named setter confirmation or rejection | `result` | Once for the matching `command_id` |
| Setting read-back after a confirmed setter | `snapshot/#` | Immediately after confirmation; replaces that retained setting |

### Polling and delivery timing

| Poll class | Interval | Typical content |
|---|---:|---|
| Critical | 1 second | Operating state and inverter fault/warning data |
| Fast | 5 seconds | Grid, EPS, PV, battery/BMS, power flow and parallel data |
| Normal | 30 seconds | Extended PV, generator, fan/runtime and normal health data |
| Slow | 300 seconds | Energy counters |
| Boot/setup snapshot | Once per inverter communication session | Serial/model/firmware and configuration |

An alert cannot be emitted faster than the poll containing its source value. Inverter fault changes are normally detected within about one second; BMS fault/warning changes are normally detected within about five seconds.

### Terminus/Linux subscriptions

Subscribe to all published streams for one device installation using one MQTT connection:

```bash
mosquitto_sub -h "<broker>" -q 1 -v \
  -t "<prefix>/<device-id>/inverter/+/live" \
  -t "<prefix>/<device-id>/inverter/+/alert/#" \
  -t "<prefix>/<device-id>/inverter/+/result" \
  -t "<prefix>/<device-id>/inverter/+/snapshot/#" \
  -t "<prefix>/<device-id>/inverter/+/availability"
```

The equivalent compact one-device wildcard is:

```bash
mosquitto_sub -h "<broker>" -q 1 -v -t "<prefix>/<device-id>/inverter/#"
```

This compact testing wildcard also displays command messages published under the same inverter tree. Use the explicit stream filters above when the subscriber must receive ESP output only.

Separate terminal subscriptions:

```bash
# Repeated live measurements
mosquitto_sub -h "<broker>" -q 1 -v -t "<prefix>/<device-id>/inverter/+/live"

# Current and changing warnings/faults
mosquitto_sub -h "<broker>" -q 1 -v -t "<prefix>/<device-id>/inverter/+/alert/#"

# Getter responses and setter confirmations
mosquitto_sub -h "<broker>" -q 1 -v -t "<prefix>/<device-id>/inverter/+/result"

# Retained serial/model/firmware/configuration data
mosquitto_sub -h "<broker>" -q 1 -v -t "<prefix>/<device-id>/inverter/+/snapshot/#"
```

For a central server handling every device installation, use one persistent wildcard subscription rather than one connection per inverter:

```bash
mosquitto_sub -h "<broker>" -q 1 -v \
  -t "<prefix>/+/inverter/+/live" \
  -t "<prefix>/+/inverter/+/alert/#" \
  -t "<prefix>/+/inverter/+/result" \
  -t "<prefix>/+/inverter/+/snapshot/#" \
  -t "<prefix>/+/inverter/+/availability"
```

MQTT wildcard meaning:

- `+` matches exactly one topic level, such as one device installation or one inverter member.
- `#` matches the remaining topic levels, including all snapshot and alert subtopics.
- Add `-u "<username>" -P "<password>"` when broker authentication is enabled.
- Add `--cafile <ca-file>` and use the configured TLS listener for a production broker.
- Add `-R` when testing only future events and you intentionally do not want retained snapshot/alert state.

For more than one thousand inverters, use one long-lived MQTT client connection with wildcard subscriptions. Do not start one `mosquitto_sub` process or one broker connection per inverter; the CLI commands here are for commissioning and testing.

### Stream message illustrations

Example live message:

```json
{
  "member_id": 0,
  "sequence": 42,
  "uptime_ms": 180500,
  "data": [
    {
      "name": "Vbat",
      "description": "Battery Voltage",
      "domain": "BATTERY",
      "unit": "0.1V",
      "value": 41.5
    }
  ]
}
```

Example retained warning/fault state:

```json
{
  "member_id": 0,
  "sequence": 43,
  "uptime_ms": 181000,
  "severity": "warning",
  "state": "active",
  "name": "InvMainWarnCode",
  "description": "Inverter Main Warning Code",
  "previous_value": 0,
  "value": 12
}
```

When its value returns to zero, the same retained alert topic publishes `"state":"clear"`. Inverter system-fault words, main/sub fault and warning codes, derating state, BMS status, BMS errors and BMS warnings are monitored for changes.

Example retained serial-number snapshot:

```json
{
  "member_id": 0,
  "sequence": 2,
  "uptime_ms": 5200,
  "name": "SerialNumber",
  "description": "Inverter serial number",
  "domain": "IDENTITY",
  "value": "EXAMPLE123456789"
}
```

The serial-number words are assembled inside the ESP. Other model, firmware and configuration values are retained as individually named snapshot messages.

After a confirmed setter, `result` first reports the correlated command result and the corresponding retained snapshot is refreshed with `updated_by_command_id`:

```json
{
  "member_id": 0,
  "updated_by_command_id": "920001",
  "name": "DspBeepOnOff",
  "description": "Buzzer switch",
  "domain": "SYSTEM",
  "value": 1
}
```

## Getter command: read data from the inverter

### Normal named inquiry

The server does not need the Modbus address. It sends the exact English catalog variable name:

```json
{
  "command_id": "20001",
  "operation": "get",
  "category": "input_data",
  "name": "Vbat"
}
```

The MQTT gateway resolves `Vbat` to its input-register descriptor before routing the compact command to the inverter ESP. Names are case-sensitive. If the workbook contains the same name more than once in one register space, add its catalog `description` and/or `domain`:

```json
{
  "command_id": "20002",
  "operation": "get",
  "category": "input_data",
  "name": "SOC",
  "description": "Battery SOC",
  "domain": "battery"
}
```

The domain distinguishes local battery `SOC` from parallel aggregate `SOC`. Valid domain strings are `identity`, `communications`, `system`, `inverter`, `grid`, `eps`, `pv`, `generator`, `battery`, `bms`, `load`, `power`, `energy`, `parallel`, `diagnostics`, and `reserved`.

A named 32-bit item is assembled internally. MQTT never supplies its address, high word, low word or scale.

A successful result contains the resolved catalog identity and decoded application value. It does not expose a Modbus address or raw register word:

```json
{
  "member_id": 0,
  "command_id": "20001",
  "operation": "get",
  "outcome": "confirmed",
  "name": "Vbat",
  "description": "Battery Voltage",
  "domain": "BATTERY",
  "unit": "0.1V",
  "value": 41.5
}
```

Getter lifecycle:

```text
server publishes get on command
-> selected ESP reads the inverter
-> ESP publishes exactly one result with the same command_id
```

A getter response is not retained. Subscribe to `result` before publishing the getter. Use `snapshot/#` when the server needs retained identity/configuration immediately after connecting; use a getter when it needs a fresh on-demand read.

## Setter command: write a setting or command to the inverter

### Normal named setter

```json
{
  "command_id": "20004",
  "operation": "set",
  "name": "DspBeepOnOff",
  "value": true,
  "confirmed": true
}
```

The setter contains the desired application value, never a Modbus address or expected raw register word. The ESP resolves the setting, converts engineering units or boolean values to the inverter encoding, reads the current value itself, writes through the applicable guard and verifies the result.

The service performs:

```text
resolve setting name -> encode application value -> write-guard/state check
-> read current inverter value
-> FC06 for one word or FC10 for multiple words
-> read back -> decode application value -> publish result
```

Setter lifecycle:

```text
server publishes set on command
-> ESP validates name, value, state and required interlock
-> ESP reads the current value
-> ESP writes only when a change is needed
-> ESP reads back the setting
-> ESP publishes result with the same command_id
-> if confirmed, ESP refreshes the retained snapshot for that setting
```

The requester should treat `result` as the command acknowledgement. The refreshed `snapshot/#` message is the durable current configuration broadcast for all subscribers and for clients that connect later.

Rules:

- Every setting name must resolve uniquely to a documented writable setting.
- `confirmed: true` is mandatory.
- The MQTT bridge remembers the most recent 32 command IDs and rejects duplicates during the current boot. Use a new nonzero ID for every deliberate request.
- Boolean settings accept `true` or `false`; numeric settings accept engineering values; two-character ASCII cells accept a string.
- The ESP handles signed encoding, scale, and complete multiword writes internally.
- The server must still validate documented ranges, enums and dependencies before requesting a safety-sensitive change.

## Write-guard requirements

| Guard | MQTT behavior |
|---|---|
| `read_only` | Rejected. |
| `blocked_unvalidated` | Rejected. |
| `hot_edit_confirmed` | Confirmation and internal read-back required. |
| `hot_edit_guarded` | Confirmation, documented dependency validation and `"guarded_interlock": true` required. |
| `runtime_command` | Requires fresh non-fault/non-update state, except an inverter-off request; unvalidated H0 values are rejected. |
| `standby_required` | Requires fresh `standby` or `stopped` state. |
| `commissioning_only` | Requires fresh standby/stopped state and `"commissioning_interlock": true`. |
| `service_only` | Requires fresh standby/stopped state and `"service_interlock": true`. |

Example commissioning setter:

```json
{
  "command_id": "20007",
  "operation": "set",
  "name": "fGridVoltLow1EE",
  "value": 180.0,
  "confirmed": true,
  "commissioning_interlock": true
}
```

Do not use sample values as an installation prescription. Parallel, battery/BMS, grid-code, protection, reset and firmware-update settings require the exact inverter model documentation and qualified commissioning procedure.

## Outcomes

| Outcome | Meaning |
|---|---|
| `confirmed` | Read completed, write was already at the requested value, or write read-back matched. |
| `sent_unverifiable` | A write-only service action received a valid Modbus response but cannot be read back. Never retry automatically. |
| `not_documented` | The requested name is absent from the catalog. |
| `not_readable` | The requested name identifies a write-only command. |
| `read_only` | The requested name is not writable. |
| `blocked_unvalidated` | The catalog/model policy intentionally forbids this write. |
| `confirmation_required` | `confirmed: true` was missing. |
| `state_unavailable` | Fresh operating state is unavailable. |
| `state_not_allowed` | Current operating state does not satisfy the guard. |
| `guarded_interlock_required` | A guarded runtime-policy acknowledgement is missing. |
| `commissioning_interlock_required` | Protected commissioning acknowledgement is missing. |
| `service_interlock_required` | Protected service acknowledgement is missing. |
| `communication_error` | Modbus transmit, response, CRC, exception, echo or read-back failed. |
| `invalid_request` | Count/range, pair completeness or write-only grouping is invalid. |

## Coverage and limitations

The internal catalog contains 265 configuration rows and 422 input-data rows. Every uniquely named, readable application item has a public getter; catalog rows that are only the second half of a value are assembled internally. Every named writable setting allowed by policy reaches the common guard engine. This does not override its safety class: read-only and blocked items remain non-writable, and the inverter may reject unsupported model-specific features.

The current transport security is not production-complete: ESP-NOW payload encryption, verified MQTT TLS certificate provisioning, user authorization levels, replay protection that survives reboot and hardware validation on every writable register remain required before unattended deployment.

## Illustrated Mosquitto workflow

```text
operator/backend                  Mosquitto                     target ESP/inverter
       |                             |                                  |
       |-- publish get/set --------->|                                  |
       |                             |-- route device/member command -->|
       |                             |                                  |-- Modbus read/write
       |                             |                                  |-- verify/read back
       |                             |<-- publish correlated result ----|
       |<-- subscribed result -------|                                  |
```

### Windows PowerShell illustration

```powershell
$Broker = 'mqtt.example.com'
$Prefix = 'inverter'
$DeviceId = 'INV-AABBCCDDEEFF'
$Member = 0
$CommandTopic = "$Prefix/$DeviceId/inverter/$Member/command"
$ResultTopic = "$Prefix/$DeviceId/inverter/$Member/result"

mosquitto_sub.exe -h $Broker -q 1 -v `
  -t "$Prefix/$DeviceId/inverter/+/live" `
  -t "$Prefix/$DeviceId/inverter/+/alert/#" `
  -t "$Prefix/$DeviceId/inverter/+/result" `
  -t "$Prefix/$DeviceId/inverter/+/snapshot/#" `
  -t "$Prefix/$DeviceId/inverter/+/availability"
```

In a second PowerShell window, read battery voltage:

```powershell
$Payload = '{"command_id":"910001","operation":"get","category":"input_data","name":"Vbat"}'
mosquitto_pub.exe -h $Broker -q 1 -t $CommandTopic -m $Payload
```

Enable the buzzer:

```powershell
$Payload = '{"command_id":"920001","operation":"set","name":"DspBeepOnOff","value":true,"confirmed":true}'
mosquitto_pub.exe -h $Broker -q 1 -t $CommandTopic -m $Payload
```

### Linux/macOS shell illustration

Set the connection values and copy the hardware-generated device ID reported by the ESP32:

```bash
BROKER="mqtt.example.com"
PREFIX="inverter"
DEVICE_ID="INV-AABBCCDDEEFF"
MEMBER="0"
COMMAND_TOPIC="$PREFIX/$DEVICE_ID/inverter/$MEMBER/command"
RESULT_TOPIC="$PREFIX/$DEVICE_ID/inverter/$MEMBER/result"
```

Keep all stream subscriptions running:

```bash
mosquitto_sub -h "$BROKER" -q 1 -v \
  -t "$PREFIX/$DEVICE_ID/inverter/+/live" \
  -t "$PREFIX/$DEVICE_ID/inverter/+/alert/#" \
  -t "$PREFIX/$DEVICE_ID/inverter/+/result" \
  -t "$PREFIX/$DEVICE_ID/inverter/+/snapshot/#" \
  -t "$PREFIX/$DEVICE_ID/inverter/+/availability"
```

Read grid voltage:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"910001","operation":"get","category":"input_data","name":"Vac-R"}'
```

Read battery voltage:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"910002","operation":"get","category":"input_data","name":"Vbat"}'
```

Read local battery SOC, distinguished from parallel SOC:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"910003","operation":"get","category":"input_data","name":"SOC","domain":"battery"}'
```

Read PV1 voltage and PV1 power. `Ppv1 H` is a 32-bit point, so the ESP automatically includes its low word:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"910004","operation":"get","category":"input_data","name":"Vpv1"}'

mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"910005","operation":"get","category":"input_data","name":"Ppv1 H"}'
```

Enable the buzzer:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920001","operation":"set","name":"DspBeepOnOff","value":true,"confirmed":true}'
```

Enable Bluetooth:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920002","operation":"set","name":"ubBluetoothEn","value":true,"confirmed":true}'
```

Turn the inverter off:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920003","operation":"set","name":"OnOffSet","value":false,"confirmed":true}'
```

Change battery type only after the inverter reports a fresh standby/stopped state. This example selects documented mode `0`; it is an API illustration, not a battery recommendation:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920004","operation":"set","name":"BatteryType","value":0,"confirmed":true}'
```

Change a guarded operating-policy setting after validating its dependencies:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920005","operation":"set","name":"OverloadToBypass","value":true,"confirmed":true,"guarded_interlock":true}'
```

A commissioning-only grid setting additionally requires a fresh standby/stopped inverter and the commissioning interlock:

```bash
mosquitto_pub -h "$BROKER" -q 1 -t "$COMMAND_TOPIC" -m \
'{"command_id":"920006","operation":"set","name":"fGridVoltLow1EE","value":180.0,"confirmed":true,"commissioning_interlock":true}'
```

Typical confirmed setter result:

```json
{
  "member_id": 0,
  "command_id": "920001",
  "operation": "set",
  "outcome": "confirmed",
  "name": "DspBeepOnOff",
  "description": "Buzzer switch",
  "domain": "SYSTEM",
  "write_guard": "HOT_EDIT_CONFIRMED",
  "previous_value": 0,
  "value": 1
}
```

<!-- BEGIN GENERATED COMPLETE COMMAND CATALOG -->
## Complete generated Mosquitto command catalog

This section is generated from the internal catalog. Replace `<broker>`, `<prefix>`, `<device-id>`, and `<member-id>`. Every displayed `command_id` is illustrative and must be replaced by a fresh globally unique positive ID before each publication. Replace `<new-value>` with the intended application-level JSON value: a boolean, an engineering-unit number, a documented mode number, or a one/two-character string as appropriate. MQTT clients never send Modbus addresses, register spaces, raw words, scaling values, or expected register values.

Add `-u "<username>" -P "<password>"` when authentication is enabled. Subscribe to command results before publishing. Use the `live`, `alert/#`, and `snapshot/#` subscriptions documented above for asynchronous data:

```bash
mosquitto_sub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/result"
```

### GETTERS - read data from the inverter

These public commands contain only an application category and catalog data name. Modbus address, function, scale conversion and word layout remain inside the ESP firmware.

#### Configuration getters (current setup, identity and command state)

| Name | Description | Domain | Type/unit | Read class | Mosquitto getter command |
|---|---|---|---|---|---|
| OnOffSet | Switch on/off | system | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"OnOffSet"}'` |
| SystemSetBit | System Enable Flag Bit | system | unknown | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SystemSetBit"}'` |
| DeviceType | Device Type | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"DeviceType"}'` |
| DeviceID | Device Correspondence Address | communications | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"DeviceID"}'` |
| BaudrateSet | Select Baud Rate | communications | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BaudrateSet"}'` |
| Serial NO.1 | Serial number 1 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.1"}'` |
| Serial NO.2 | Serial number 2 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.2"}'` |
| Serial NO.3 | Serial number 3 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.3"}'` |
| Serial NO.4 | Serial numbe 4 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.4"}'` |
| Serial NO.5 | Serial number 5 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.5"}'` |
| Serial NO.6 | Serial number 6 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.6"}'` |
| Serial NO.7 | Serial numbe 7 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.7"}'` |
| Serial NO.8 | Serial numbe 8 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.8"}'` |
| Serial NO.9 | Serial number 9 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.9"}'` |
| Serial NO.10 | Serial number 10 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.10"}'` |
| Serial NO.11 | Serial number 11 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.11"}'` |
| Serial NO.12 | Serial number 12 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.12"}'` |
| Serial NO.13 | Serial number 13 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.13"}'` |
| Serial NO.14 | Serial number 14 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.14"}'` |
| Serial NO.15 | Serial numbe 15 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Serial NO.15"}'` |
| Rs485WorkMode | 485 Operating Mode Selection | communications | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Rs485WorkMode"}'` |
| Year | System Time - Year | system | uint16; Year | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Year"}'` |
| Month | System Time - Month | system | uint16; Month | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Month"}'` |
| Day | System Time - Day | system | uint16; Day | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Day"}'` |
| Hour | System Time - Hours | system | uint16; Hour | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Hour"}'` |
| Minute | System Time - Minutes | system | uint16; Minutes | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Minute"}'` |
| Second | System Time - Seconds | system | uint16; Seconds | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Second"}'` |
| Weekday | System Week | system | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Weekday"}'` |
| SoftVersion_Attest | Firmware Version (High) | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SoftVersion_Attest"}'` |
| SoftVersion_Monitor | Arm Firmware Version Name | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SoftVersion_Monitor"}'` |
| SoftVersion_Control | DSP Software Version Name (TJ) | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SoftVersion_Control"}'` |
| MasterDSPTestVersion | DSP1 software debug version number | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"MasterDSPTestVersion"}'` |
| SlaveDSPTestVersion | DSP2 software debug version number | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SlaveDSPTestVersion"}'` |
| ARM TestVersion | Arm debug version number | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ARM TestVersion"}'` |
| DSP Hard version | Dashboard Hardware Version | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"DSP Hard version"}'` |
| ARM Hard version | Dashboard Hardware Version | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ARM Hard version"}'` |
| Manufacturer Info 8 | Manufacturer Information 8 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 8"}'` |
| Manufacturer Info 7 | Manufacturer Information 7 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 7"}'` |
| Manufacturer Info 6 | Manufacturer Information 6 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 6"}'` |
| Manufacturer Info 5 | Manufacturer Information 5 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 5"}'` |
| Manufacturer Info 4 | Manufacturer Information 4 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 4"}'` |
| Manufacturer Info3 | Manufacturer Information 3 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info3"}'` |
| Manufacturer Info 2 | Manufacturer Info 2 | identity | ascii; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 2"}'` |
| Manufacturer Info 1 | Manufacturer Info 1 | identity | ascii_or_uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Manufacturer Info 1"}'` |
| ModbusVersion | Modbus Version | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ModbusVersion"}'` |
| Module 4 | Inverter Mode (4) | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Module 4"}'` |
| Module 3 | Inverter Mode (3) | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Module 3"}'` |
| Module 2 | Inverter Mode (2) | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Module 2"}'` |
| Module 1 | Inverter Mode (1) | identity | uint16; - | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Module 1"}'` |
| MaxInvPower | Rated power (high) | identity | uint16; 0.1W | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"MaxInvPower"}'` |
| UpdateState | Firmware update progress | system | uint16; - | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"UpdateState"}'` |
| ubScreenType | Screen Type | system | uint8; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubScreenType"}'` |
| SafetyId | Safety regulation number | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SafetyId"}'` |
| ReactivePowerDelayTIme | Reactive Percent Response Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ReactivePowerDelayTIme"}'` |
| ActiveOverloadEnable | Active Power Overload Enabled | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ActiveOverloadEnable"}'` |
| AcChargePowerRate | Inverter Max Take-Off Power Percentage | grid | uint16; 1%Pn | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AcChargePowerRate"}'` |
| ActivePowerSlope | Active power rate of change (N4105) | grid | uint16; 0.1Pn/min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ActivePowerSlope"}'` |
| ActivePowerRate | Inverter Max Output Active Power Percentage | grid | uint16; 1%Pn | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ActivePowerRate"}'` |
| ReactivePowerRate | Inverter Max Output Reactive Power Percentage | grid | sint16; 1%Pn | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ReactivePowerRate"}'` |
| PowerFactorSet | Inverter output power factor 10,000 times | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PowerFactorSet"}'` |
| PVModelSelect | MPPT Mode | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PVModelSelect"}'` |
| PvStartVoltage | PV start voltage | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PvStartVoltage"}'` |
| FastDerating_en | Fast Down Enable | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"FastDerating_en"}'` |
| Island_en | Island Enabling | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Island_en"}'` |
| VFRT_en | High and low penetration enable | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"VFRT_en"}'` |
| DRMS_en | DRMS Enabled | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"DRMS_en"}'` |
| NLineConnectMode | N-line enable | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NLineConnectMode"}'` |
| NToGNDDetect | Zero Earth Detection | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NToGNDDetect"}'` |
| ZeroPowerOutputEnable | Zero power output enabled | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ZeroPowerOutputEnable"}'` |
| FastMpptEnable | Fast mppt enable | grid | uint16; - | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"FastMpptEnable"}'` |
| CtRatioSet | CT Rheological Ratio Setting | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"CtRatioSet"}'` |
| CTMode | Using CT Mode | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"CTMode"}'` |
| LocalAntiBackflowEnable | Anti-reflux enable | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"LocalAntiBackflowEnable"}'` |
| BackflowMeterPowerLimit_R | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowMeterPowerLimit_R"}'` |
| BackflowMeterPowerLimit_S | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowMeterPowerLimit_S"}'` |
| BackflowMeterPowerLimit_T | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowMeterPowerLimit_T"}'` |
| BackflowHostNoResponseFlag | Anti-reflux host failure flag bit | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowHostNoResponseFlag"}'` |
| BackflowFaultTime | Anti-reflux failure time | grid | uint16; 1s | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowFaultTime"}'` |
| BackflowFaultPowerRate | Percentage of active anti-reflux failure | grid | sint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BackflowFaultPowerRate"}'` |
| Power on config falg | Boot navigation flags | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Power on config falg"}'` |
| Vac low C | Grid Low Voltage Restrictions Connected to the Grid | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Vac low C"}'` |
| Vac high C | Grid High Voltage Restriction Connected to Grid | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Vac high C"}'` |
| Fac low C | Grid Low Frequency Restrictions Connected to the Grid | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Fac low C"}'` |
| Fac high C | Grid High Frequency Restrictions Connected to the Grid | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Fac high C"}'` |
| NominalEPSVolt | Optional rated off-grid voltage | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NominalEPSVolt"}'` |
| NominalEPSFre | Optional rated off-grid frequency | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NominalEPSFre"}'` |
| EPS_En | Off-grid functionality enabled | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EPS_En"}'` |
| Bypass_En | Bypass mode enabled | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Bypass_En"}'` |
| UPS_En | UPS Function Enabled | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"UPS_En"}'` |
| ubN_PE_RelayCMD | Zero earth relay control enabled | eps | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubN_PE_RelayCMD"}'` |
| CheckFanCmd | Fan Detection | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"CheckFanCmd"}'` |
| unAFCICtrlReg | AFCI Self-Test Command | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"unAFCICtrlReg"}'` |
| uwAFCIThresholdValue | AFCI error threshold | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwAFCIThresholdValue"}'` |
| AFCIClrFaultCmd | AFCI Error Clear Command | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AFCIClrFaultCmd"}'` |
| uwRestartVoltL | Fault reconnect grid low voltage | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwRestartVoltL"}'` |
| uwRestartVoltH | Fault reconnect grid high voltage | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwRestartVoltH"}'` |
| uwRestartFreqL | Fault Reconnect Grid Low Frequency | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwRestartFreqL"}'` |
| uwRestartFreqH | Fault Reconnect Grid High Frequency | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwRestartFreqH"}'` |
| BatteryType | Battery type | battery | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatteryType"}'` |
| BatteryCompanySet | Battery communication protocol selection | bms | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatteryCompanySet"}'` |
| BatMdlSerialNum | Number of batteries in series - high-voltage batteries; | battery | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatMdlSerialNum"}'` |
| BatMdlParallNum | Number of battery parallel joints; | battery | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatMdlParallNum"}'` |
| ChargeCurrentLimit | Charge Limit Current | battery | uint16; 0.01A | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ChargeCurrentLimit"}'` |
| VbatStopForDischarge | Discharge cut-off voltage | battery | uint16; 0.01V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"VbatStopForDischarge"}'` |
| Vbat constant charge | Charge cut-off voltage | battery | uint16; 0.01V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Vbat constant charge"}'` |
| ubHighVoltBatRatedVolt | Battery nominal voltage | battery | uint16; 1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubHighVoltBatRatedVolt"}'` |
| ChargeRate | Charging power | battery | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ChargeRate","domain":"battery","description":"Charging power"}'` |
| ChargeRate | Discharge power | battery | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ChargeRate","domain":"battery","description":"Discharge power"}'` |
| BatFirstStopSOC | Charge cut-off SOC | battery | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatFirstStopSOC"}'` |
| OnLineStopSOC | Grid-connected discharge cut-off SOC | battery | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"OnLineStopSOC"}'` |
| OffLineStopSoc | Off-grid Discharge Cutoff SOC | battery | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"OffLineStopSoc"}'` |
| Gen Charge En | Total Diesel Charging Enabled | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Gen Charge En"}'` |
| AcCharge_En | AC Charge Enabled | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AcCharge_En"}'` |
| LoadFirstDischargeDisableFlag | Load Preferred Discharge Prohibit Sign | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"LoadFirstDischargeDisableFlag"}'` |
| RemotePowerControl | One-click charging and discharging | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"RemotePowerControl"}'` |
| NumberTimePeriods | Number of charging and discharging periods - number of currently valid periods, starting with period 1 | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NumberTimePeriods"}'` |
| PeriodTimeStart1 | Period 1 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart1"}'` |
| PeriodTimeEnd1 | Period 1 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd1","domain":"system","description":"Period 1 End Time"}'` |
| PeriodTimeRate1 | Period 1 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate1","domain":"system","description":"Period 1 Priority"}'` |
| PeriodTimeStart2 | Period 2 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart2"}'` |
| PeriodTimeEnd2 | Period 2 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd2"}'` |
| PeriodTimeRate2 | Period 2 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate2"}'` |
| PeriodTimeStart3 | Period 3 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart3"}'` |
| PeriodTimeEnd3 | Slot 3 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd3"}'` |
| PeriodTimeRate3 | Period 3 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate3"}'` |
| PeriodTimeStart4 | Period 4 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart4"}'` |
| PeriodTimeEnd4 | Period 4 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd4"}'` |
| PeriodTimeRate4 | Period 4 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate4"}'` |
| PeriodTimeStart5 | Period 5 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart5"}'` |
| PeriodTimeEnd5 | Slot 5 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd5"}'` |
| PeriodTimeRate5 | Period 5 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate5"}'` |
| PeriodTimeStart6 | Period 6 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart6"}'` |
| PeriodTimeEnd1 | Slot 6 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd1","domain":"system","description":"Slot 6 End Time"}'` |
| PeriodTimeRate1 | Period 6 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate1","domain":"system","description":"Period 6 Priority"}'` |
| PeriodTimeStart7 | Period 7 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart7"}'` |
| PeriodTimeEnd1 | Segment 7 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd1","domain":"system","description":"Segment 7 End Time"}'` |
| PeriodTimeRate1 | Period 7 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate1","domain":"system","description":"Period 7 Priority"}'` |
| PeriodTimeStart8 | Period 8 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart8"}'` |
| PeriodTimeEnd1 | Slot 8 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd1","domain":"system","description":"Slot 8 End Time"}'` |
| PeriodTimeRate1 | Period 8 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate1","domain":"system","description":"Period 8 Priority"}'` |
| PeriodTimeStart9 | Period 9 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart9"}'` |
| PeriodTimeEnd1 | Period 9 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd1","domain":"system","description":"Period 9 End Time"}'` |
| PeriodTimeRate1 | Period 9 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate1","domain":"system","description":"Period 9 Priority"}'` |
| PeriodTimeStart10 | Period 10 Start Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeStart10"}'` |
| PeriodTimeEnd10 | Slot 10 End Time | system | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeEnd10"}'` |
| PeriodTimeRate10 | Period 10 Priority | system | uint16; 1Pn% | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PeriodTimeRate10"}'` |
| ChargeSourcePriority | Charge Priority | system | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ChargeSourcePriority"}'` |
| SourcePriority | Energy Priority | system | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SourcePriority"}'` |
| uwCC_DisChrLead_100T | Lead Acid Discharge Current | battery | uint16; 0.01A | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwCC_DisChrLead_100T"}'` |
| ubOP_RecoverDischargeSOC | Grid-connected stop discharge recovery SOC | battery | uint8; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubOP_RecoverDischargeSOC"}'` |
| ubOffGrid_RecoverDischargeSOC | Off-grid Stop Discharge Recovery SOC | battery | uint8; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubOffGrid_RecoverDischargeSOC"}'` |
| BatUnderVol | Battery Low Voltage Shutdown Voltage | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatUnderVol"}'` |
| AcChargingCurrent | AC Charge Limit Current | battery | uint16; 0.1A | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AcChargingCurrent"}'` |
| uwFloatV_Lead_100T | Floating Charge Voltage | battery | uint16; 0.01V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwFloatV_Lead_100T"}'` |
| BAT2AC_Volt | Battery-to-market voltage | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BAT2AC_Volt"}'` |
| AC2BAT_Volt | Mains to Battery Voltage | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AC2BAT_Volt"}'` |
| ubLeadAcid_BatSubType | Subtype of lead-acid battery | battery | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubLeadAcid_BatSubType"}'` |
| IncreChar_MaxTim | Improved charging time | battery | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"IncreChar_MaxTim"}'` |
| BatUnderVolt_Point | Battery undervoltage alarm point | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatUnderVolt_Point"}'` |
| Equalization | Equalization Mode Enabled | battery | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Equalization"}'` |
| EQBatteryTime | Equalize charging time | battery | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EQBatteryTime"}'` |
| EQBatteryTimeout | Equalize Charge Delay Time | battery | uint16; 1min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EQBatteryTimeout"}'` |
| EqualizationCycle | Balanced charge interval | battery | uint8; 1DAY | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EqualizationCycle"}'` |
| EqualizationImmediately | Turn on Balanced Charging now | battery | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EqualizationImmediately"}'` |
| flcdEn | LCD setting enable bit | system | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"flcdEn"}'` |
| BatLVBreak_RestartVolt | Low Voltage Disconnect Battery Recovery Point | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatLVBreak_RestartVolt"}'` |
| BatNeedChr_Volt | Battery Recharge Recovery Point | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatNeedChr_Volt"}'` |
| NonCriticlLoad_BatDisConVolt | Non-critical load disconnect battery voltage | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"NonCriticlLoad_BatDisConVolt"}'` |
| BatHighVolt_DisConPoint | Overvoltage disconnection voltage | battery | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatHighVolt_DisConPoint"}'` |
| BatOverDisCharge_Delay | Battery Overdischarge Delay Time | battery | uint16; 1s | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"BatOverDisCharge_Delay"}'` |
| DspBeepOnOff | Buzzer switch | system | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"DspBeepOnOff"}'` |
| OverloadToBypass | Overload transfer bypass enable | eps | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"OverloadToBypass"}'` |
| AcInputType | Off-grid output mode | grid | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"AcInputType"}'` |
| EQBatteryVoltage_100T | Balanced Charging Voltage | battery | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"EQBatteryVoltage_100T"}'` |
| Parallel_Mode | Parallel Mode | parallel | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Parallel_Mode"}'` |
| ubParallelDeviveID | Parallel can communication address | parallel | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubParallelDeviveID"}'` |
| ubParallelDeviveType | Parallel Device Type | parallel | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubParallelDeviveType"}'` |
| ubBMSWorkMode | BMS communication method | bms | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubBMSWorkMode"}'` |
| uwGridPowerCompensation | Grid Power Compensation | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"uwGridPowerCompensation"}'` |
| Gen Port Work Mode | Diesel Port Function Selection | generator | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Gen Port Work Mode"}'` |
| Gen Charge Curr Limit | Diesel Charging Current Limit | generator | uint16; 1A | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Gen Charge Curr Limit"}'` |
| Gen Input Rated Power | Generator Input Rated Power | generator | uint16; 10W | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Gen Input Rated Power"}'` |
| SecEPS ON SOC/Vbat | (Lithium) Start SOC | generator | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SecEPS ON SOC/Vbat"}'` |
| SecEPS ON Vbat | (Lead acid) Starting battery voltage | generator | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SecEPS ON Vbat"}'` |
| SecEPS OFF SOC/Vbat | (Lithium) Shutdown SOC | generator | uint16; 0.01 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SecEPS OFF SOC/Vbat"}'` |
| SecEPS OFF Vbat | (Lead Acid) Shutdown Battery Voltage | generator | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SecEPS OFF Vbat"}'` |
| SecEPS  On PV Power Min | Minimum power of photovoltaic startup smart load | generator | uint16; 10W | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"SecEPS  On PV Power Min"}'` |
| ubBluetoothEn | Bluetooth Enabled | system | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubBluetoothEn"}'` |
| upgrade notification | Data update notification, wifi re-submit 0304 data to the server | communications | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"upgrade notification"}'` |
| Datalogger Restart | wifi factory settings restored, server domain name and escalation time modified | communications | uint8 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Datalogger Restart"}'` |
| ubConnectServer | Collector network status | communications | uint8 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubConnectServer"}'` |
| ubDatalogAndArmCommunication | Collector and inverter communication status | communications | uint8 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"ubDatalogAndArmCommunication"}'` |
| LocalSafetyCmd | User Safety Selection Instructions | grid | uint16 | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"LocalSafetyCmd"}'` |
| fGridVoltLow1EE | Low Grid Voltage Protection Tier 1 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltLow1EE"}'` |
| fGridVoltHigh1EE | High Grid Voltage Protection Tier 1 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltHigh1EE"}'` |
| fFreqLow1EE | Grid Low Frequency Protection First Order | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqLow1EE"}'` |
| fFreqHigh1EE | Grid High Frequency Protection First Order | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqHigh1EE"}'` |
| fGridVoltLow2EE | Low Grid Voltage Protection Level 2 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltLow2EE"}'` |
| fGridVoltHigh2EE | High Grid Voltage Protection Level 2 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltHigh2EE"}'` |
| fFreqLow2EE | Grid Low Frequency Protection Second Order | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqLow2EE"}'` |
| fFreqHigh2EE | Grid High Frequency Protection Second Order | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqHigh2EE"}'` |
| fGridVoltLow3EE | Low Grid Voltage Protection Level 3 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltLow3EE"}'` |
| fGridVoltHigh3EE | High Grid Voltage Protection Level 3 | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fGridVoltHigh3EE"}'` |
| fFreqLow3EE | Grid Low Frequency Protection Level 3 | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqLow3EE"}'` |
| fFreqHigh3EE | Grid High Frequency Protection Level 3 | grid | uint16; 0.01Hz | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"fFreqHigh3EE"}'` |
| wVLowCutTime1EE | Low Grid Voltage First Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVLowCutTime1EE"}'` |
| wVHighCutTime1EE | High grid voltage first-order protection time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVHighCutTime1EE"}'` |
| udFLowCutTime1EE | Low Grid Frequency First Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFLowCutTime1EE"}'` |
| udFHighCutTime1EE | High Grid Frequency First Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFHighCutTime1EE"}'` |
| wVLowCutTime2EE | Low Grid Voltage Second Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVLowCutTime2EE"}'` |
| wVHighCutTime2EE | High Grid Voltage Second Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVHighCutTime2EE"}'` |
| udFLowCutTime2EE | Low Grid Frequency Second Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFLowCutTime2EE"}'` |
| udFHighCutTime2EE | High Grid Frequency Second Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFHighCutTime2EE"}'` |
| wVLowCutTime3EE | Low grid voltage third-order protection time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVLowCutTime3EE"}'` |
| wVHighCutTime3EE | High grid voltage third-order protection time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"wVHighCutTime3EE"}'` |
| udFLowCutTime3EE | Low Grid Frequency Third Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFLowCutTime3EE"}'` |
| udFHighCutTime3EE | High Grid Frequency Third Order Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"udFHighCutTime3EE"}'` |
| 10MinAVLimit | Voltage protection for ten minutes | grid | uint16; 0.1V | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"10MinAVLimit"}'` |
| U10minTime | 10 Minute Average Voltage Protection Time | grid | uint16; 20ms | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"U10minTime"}'` |
| Time start | Connection time | grid | uint16; 1s | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"Time start"}'` |
| RestartDelayTime | Reconnection time | grid | uint16; 1s | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"RestartDelayTime"}'` |
| PowerStartSlope | Load rate | grid | uint16; 0.1Pn%/min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PowerStartSlope"}'` |
| PowerRestartSlopeEE | Restart Load Rate | grid | uint16; 0.1Pn%/min | setup_snapshot | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"configuration","name":"PowerRestartSlopeEE"}'` |

#### Input-register getters (status, measurements, power, energy and faults)

| Name | Description | Domain | Type/unit | Read class | Mosquitto getter command |
|---|---|---|---|---|---|
| Inverter Status | Inverter operating status | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Inverter Status"}'` |
| StartDelayTime | Grid-connected countdown | inverter | uint16; 1s | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"StartDelayTime"}'` |
| INV_VolR | Inverter Voltage | inverter | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_VolR"}'` |
| INV_VolS | Inverter Voltage | inverter | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_VolS"}'` |
| INV_VolT | Inverter Voltage | inverter | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_VolT"}'` |
| INV_CurrR | Inverter Current | inverter | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_CurrR"}'` |
| INV_CurrS | Inverter Current | inverter | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_CurrS"}'` |
| INV_CurrT | Inverter Current | inverter | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"INV_CurrT"}'` |
| Bus1 Voltage | Bus1 Internal Voltage | inverter | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Bus1 Voltage","domain":"inverter"}'` |
| Bus2 Voltage | Bus2 Internal Voltage | inverter | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Bus2 Voltage"}'` |
| Inv_Temp | Inverter Temperature | inverter | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Inv_Temp"}'` |
| Boost_Temp | Inverter Internal IPM Temperature | inverter | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Boost_Temp"}'` |
| LLC _Temp | LLC Radiator Temperature | inverter | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"LLC _Temp"}'` |
| Bat_Temp | Battery temperature | inverter | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Bat_Temp"}'` |
| TA_Temp | Ambient temperature | inverter | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"TA_Temp"}'` |
| DCV-R | R-phase DC voltage component | inverter | sint16; 1mV | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCV-R"}'` |
| DCV-S | S-phase DC voltage component | inverter | sint16; 1mV | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCV-S"}'` |
| DCV-T | T-phase DC voltage component | inverter | sint16; 1mV | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCV-T"}'` |
| DCI-R | R-phase DC current component | inverter | sint16; 1mA | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCI-R"}'` |
| DCI-S | S-phase DC current component | inverter | sint16; 1mA | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCI-S"}'` |
| DCI-T | T-phase DC current component | inverter | sint16; 1mA | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DCI-T"}'` |
| ISO Resistance | ISO Resistance | inverter | uint16; 1kohm | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ISO Resistance"}'` |
| GFCI | Leakage current | inverter | uint16; 1mA | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"GFCI"}'` |
| HistoryEventCnt | Number of historical event records | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"HistoryEventCnt"}'` |
| Systemfault word0 | System Failure word0 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word0"}'` |
| Systemfault word1 | System fault word1 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word1"}'` |
| Systemfault word2 | System fault word2 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word2"}'` |
| Systemfault word3 | System failure word3 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word3"}'` |
| Systemfault word4 | System failure word4 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word4"}'` |
| Systemfault word5 | System Failure word5 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word5"}'` |
| Systemfault word6 | System Failure word6 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word6"}'` |
| Systemfault word7 | System Failure word7 | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Systemfault word7"}'` |
| InvMainErrorCode | Inverter Master Fault Code | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"InvMainErrorCode"}'` |
| InvMainWarnCode | Inverter Main Warning Code | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"InvMainWarnCode"}'` |
| InvErrorSubCode | Inverter sub-fault code | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"InvErrorSubCode"}'` |
| InvWarnSubCode | Inverter sub-warning code | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"InvWarnSubCode"}'` |
| DeviceType | Device Type | inverter | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DeviceType"}'` |
| DeratingModeFlag | Download Mode Flag | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DeratingModeFlag"}'` |
| PowerCosFlag | Leading Lag Flag | inverter | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PowerCosFlag"}'` |
| Bus1 Voltage | Positive Bus Voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Bus1 Voltage","domain":"grid","description":"Positive Bus Voltage"}'` |
| Bus1 Voltage | Negative Bus Voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Bus1 Voltage","domain":"grid","description":"Negative Bus Voltage"}'` |
| Vac-R | Three Phase Grid Voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac-R"}'` |
| Iac-R | Three-phase grid output current | grid | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Iac-R"}'` |
| Vac-S | Three Phase Grid Voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac-S"}'` |
| Iac-S | Three-phase grid output current | grid | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Iac-S"}'` |
| Vac-T | Three Phase Grid Voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac-T"}'` |
| Iac-T | Three-phase grid output current | grid | sint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Iac-T"}'` |
| Vac_RS | Three-phase grid line voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac_RS"}'` |
| Vac_ST | Three-phase grid line voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac_ST"}'` |
| Vac_TR | Three-phase grid line voltage | grid | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vac_TR"}'` |
| Fac | Grid Frequency | grid | uint16; 0.01Hz | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Fac"}'` |
| PF | Power Factor | grid | sint16; 1E-4 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PF"}'` |
| RealOPPercent | Actual Output Power Percentage | grid | uint16; 0.01 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"RealOPPercent"}'` |
| EPS Fac | Off-grid frequency | eps | uint16; 0.01Hz | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Fac"}'` |
| EPS Vac1 | Off-grid R phase output voltage | eps | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Vac1"}'` |
| EPS Iac1 | Off-grid R phase output current | eps | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Iac1"}'` |
| EPS Vac2 | Off-grid S phase output voltage | eps | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Vac2"}'` |
| EPS Iac2 | Off-grid S phase output current | eps | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Iac2"}'` |
| EPS Vac3 | Off-grid T-phase output voltage | eps | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Vac3"}'` |
| EPS Iac3 | Off-grid T-phase output current | eps | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Iac3"}'` |
| PvNum | PV Paths | pv | uint16 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PvNum"}'` |
| Vpv1 | PV1 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv1"}'` |
| PV1Curr | PV1 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV1Curr"}'` |
| Vpv2 | PV2 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv2"}'` |
| PV2Curr | PV2 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV2Curr"}'` |
| Vpv3 | PV3 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv3"}'` |
| PV3Curr | PV3 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV3Curr"}'` |
| Vpv4 | PV4 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv4"}'` |
| PV4Curr | PV4 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV4Curr"}'` |
| Vpv5 | PV5 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv5"}'` |
| PV5Curr | PV5 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV5Curr"}'` |
| Vpv6 | PV6 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv6"}'` |
| PV6Curr | PV6 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV6Curr"}'` |
| Vpv7 | PV7 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv7"}'` |
| PV7Curr | PV7 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV7Curr"}'` |
| Vpv8 | PV8 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv8"}'` |
| PV8Curr | PV8 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV8Curr"}'` |
| Vpv9 | PV9 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv9"}'` |
| PV9Curr | PV9 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV9Curr"}'` |
| Vpv10 | PV10 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv10"}'` |
| PV10Curr | PV10 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV10Curr"}'` |
| Vpv11 | PV11 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv11"}'` |
| PV11Curr | PV11 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV11Curr"}'` |
| Vpv12 | PV12 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv12"}'` |
| PV12Curr | PV12 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV12Curr"}'` |
| Vpv13 | PV13 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv13"}'` |
| PV13Curr | PV13 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV13Curr"}'` |
| Vpv14 | PV14 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv14"}'` |
| PV14Curr | PV14 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV14Curr"}'` |
| Vpv15 | PV15 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv15"}'` |
| PV15Curr | PV15 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV15Curr"}'` |
| Vpv16 | PV16 Voltage | pv | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vpv16"}'` |
| PV16Curr | PV16 Input Current | pv | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PV16Curr"}'` |
| uwGEN_v_R | Diesel generator voltage R | generator | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_v_R"}'` |
| uwGEN_i_R | Diesel generator current R | generator | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_i_R"}'` |
| uwGEN_v_S | Diesel generator voltage S | generator | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_v_S"}'` |
| uwGEN_i_S | Diesel generator current S | generator | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_i_S"}'` |
| uwGEN_v_T | Diesel generator voltage T | generator | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_v_T"}'` |
| uwGEN_i_T | Diesel generator current T | generator | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_i_T"}'` |
| uwGEN_Freq | Diesel Generator Frequency | generator | uint16; 0.01Hz | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwGEN_Freq"}'` |
| fan_speed | Fan speed | system | uint16; rpm | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"fan_speed"}'` |
| Time total H | Total working hours | system | uint32; 0.5min | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Time total H"}'` |
| Debuge1 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge1"}'` |
| Debuge2 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge2"}'` |
| Debuge3 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge3"}'` |
| Debuge4 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge4"}'` |
| Debuge5 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge5"}'` |
| Debuge6 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge6"}'` |
| Debuge7 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge7"}'` |
| Debuge8 | Main DSP debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge8"}'` |
| Debuge9 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge9"}'` |
| Debuge10 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge10"}'` |
| Debuge11 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge11"}'` |
| Debuge12 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge12"}'` |
| Debuge13 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge13"}'` |
| Debuge14 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge14"}'` |
| Debuge15 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge15"}'` |
| Debuge16 | Debug parameters from DSP | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Debuge16"}'` |
| Priority | Current Battery Priority | battery | uint16 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Priority"}'` |
| Battery Type | Battery type | battery | uint16 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Battery Type"}'` |
| Vbat | Battery Voltage | battery | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Vbat"}'` |
| SOC | Battery SOC | battery | uint16; 0.01 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SOC","domain":"battery"}'` |
| BatVolt_DSP | DSP Sampling Battery Voltage | battery | uint16; 0.1V | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatVolt_DSP"}'` |
| BMS_Status | Battery status | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_Status"}'` |
| BMS_Error1 | Battery error message 1 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_Error1"}'` |
| BMS_Error2 | Battery error message 2 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_Error2"}'` |
| BMS_Error3 | Battery error message 3 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_Error3"}'` |
| BMS_Error4 | Battery error message 4 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_Error4"}'` |
| BMS_WarnInfo1 | Battery Alarm 1 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_WarnInfo1"}'` |
| BMS_WarnInfo2 | Battery Alarm 2 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_WarnInfo2"}'` |
| BMS_WarnInfo3 | Battery Alarm 3 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_WarnInfo3"}'` |
| BMS_WarnInfo4 | Battery Alarm 4 | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_WarnInfo4"}'` |
| BMS_BatteryCurr | Battery current | bms | sint16; 0.01A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_BatteryCurr"}'` |
| BMS_BatteryTemp | Battery temperature | bms | sint16; 0.1C deg | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_BatteryTemp"}'` |
| BMS_MaxChargeCurr | Maximum allowable charging current of the battery | bms | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_MaxChargeCurr"}'` |
| BMS_MaxDischargeCurr | Maximum allowable discharge current of the battery | bms | uint16; 0.1A | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_MaxDischargeCurr"}'` |
| BMS_GaugeFCC | Battery rated capacity | bms | uint16; 0.1Ah | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_GaugeFCC"}'` |
| BMS_GaugeRM | Real-Time Battery Capacity | bms | uint16; 0.1Ah | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_GaugeRM"}'` |
| BMS_SoftVersion_Major | Battery Upload Software Major Version | bms | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_SoftVersion_Major"}'` |
| BMS_SoftVersion_Minor | Battery Upload Software Minor Version | bms | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_SoftVersion_Minor"}'` |
| BMS_HardVersion | Battery Upload Hardware Version | bms | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_HardVersion"}'` |
| BMS_DeltaVolt | Battery unit pressure difference | bms | uint16; 0.001V | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_DeltaVolt"}'` |
| BMS_CycleCnt | Battery Cycles | bms | uint16 | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_CycleCnt"}'` |
| BMS_SOH | SOH | bms | uint16 | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_SOH"}'` |
| BMS_ConstantVolt | Battery Recommended Charging Voltage | bms | uint16; 0.1V | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_ConstantVolt"}'` |
| uwLVVoltage_Pack | LV Voltage | bms | uint16; 0.1V | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwLVVoltage_Pack"}'` |
| BMS_BMSInfo | BMS Information | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_BMSInfo"}'` |
| BMS_PackInfo | Pack Info | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMS_PackInfo"}'` |
| MaxCellVol | Battery maximum cell voltage | bms | uint16; 0.001V | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MaxCellVol"}'` |
| MinCellVol | Battery minimum cell voltage | bms | uint16; 0.001V | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MinCellVol"}'` |
| ModuleNum | Number of batteries in parallel | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ModuleNum"}'` |
| CellNum | Number of battery cells | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"CellNum"}'` |
| MaxVoltCellNo | Highest voltage unit number | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MaxVoltCellNo"}'` |
| MinVoltCellNo | Minimum Voltage Cell Number | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MinVoltCellNo"}'` |
| MaxTemprCell_10T | Maximum monomer temperature | bms | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MaxTemprCell_10T"}'` |
| MinTemprCell_10T | Minimum monomer temperature | bms | sint16; 0.1C deg | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MinTemprCell_10T"}'` |
| MaxTemprCellNo | Maximum Voltage Temperature Number | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MaxTemprCellNo"}'` |
| MinTemprCellNo | Minimum Voltage Temperature Number | bms | uint16 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MinTemprCellNo"}'` |
| Protect pack ID | Faulty Battery Address | bms | uint16 | critical_status | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Protect pack ID"}'` |
| MaxSOC | Parallel Maximum SOC | bms | uint16; 0.01 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MaxSOC"}'` |
| MinSOC | Parallel Minimum SOC | bms | uint16; 0.01 | live_health | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"MinSOC"}'` |
| BMSCompany | Battery manufacturer information | bms | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BMSCompany"}'` |
| PowerPackSn | Throughput Display Battery Pack Number | battery | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PowerPackSn"}'` |
| DisChargPower H | Cumulative discharge | battery | uint32; 0.1kwh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"DisChargPower H"}'` |
| ChargPower H | Cumulative Charges | battery | uint32; 0.1kwh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ChargPower H"}'` |
| BatDebuge1 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge1"}'` |
| BatDebuge2 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge2"}'` |
| BatDebuge3 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge3"}'` |
| BatDebuge4 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge4"}'` |
| BatDebuge5 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge5"}'` |
| BatDebuge6 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge6"}'` |
| BatDebuge7 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge7"}'` |
| BatDebuge8 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge8"}'` |
| BatDebuge9 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge9"}'` |
| BatDebuge10 | Battery BMS debug parameters | diagnostics | uint16 | diagnostic | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"BatDebuge10"}'` |
| PpvAll H | PV Total Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PpvAll H"}'` |
| Ppv1 H | PV1 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv1 H"}'` |
| Ppv2 H | PV2 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv2 H"}'` |
| Ppv3 H | PV3 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv3 H"}'` |
| Ppv4 H | PV4 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv4 H"}'` |
| Ppv5 H | PV5 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv5 H"}'` |
| Ppv6 H | PV6 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv6 H"}'` |
| Ppv7 H | PV7 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv7 H"}'` |
| Ppv8 H | PV8 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv8 H"}'` |
| Ppv9 H | PV9 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv9 H"}'` |
| Ppv10 H | PV10 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv10 H"}'` |
| Ppv11 H | PV11 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv11 H"}'` |
| Ppv12 H | PV12 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv12 H"}'` |
| Ppv13 H | PV13 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv13 H"}'` |
| Ppv14 H | PV14 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv14 H"}'` |
| Ppv15 H | PV15 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv15 H"}'` |
| Ppv16 H | PV16 Input Power | pv | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Ppv16 H"}'` |
| SPacAll H | Three-phase output apparent power all | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SPacAll H"}'` |
| ActPacAll H | Three-phase output power all | power | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ActPacAll H"}'` |
| ReActPacAll H | Three-phase output reactive power all | power | sint32; 0.1var | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ReActPacAll H"}'` |
| SPac_R H | Three-phase output apparent power R | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SPac_R H"}'` |
| ActPac_R H | Three-phase output power R | power | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ActPac_R H"}'` |
| ReActPac_R H | Three-phase output reactive power R | power | sint32; 0.1var | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ReActPac_R H"}'` |
| SPac_S H | Three-phase output apparent power S | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SPac_S H"}'` |
| ActPac_S H | Three-phase output power S | power | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ActPac_S H"}'` |
| ReActPac_S H | Three-phase output reactive power S | power | sint32; 0.1var | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ReActPac_S H"}'` |
| SPac_T H | Three-phase output apparent power T | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SPac_T H"}'` |
| ActPac_T H | Three-phase output power T | power | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ActPac_T H"}'` |
| ReActPac_T H | Three-phase output reactive power T | power | sint32; 0.1var | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ReActPac_T H"}'` |
| Pactouser R   H | R phase to user power | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactouser R   H"}'` |
| Pactouser S   H | S phase to user power | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactouser S   H"}'` |
| Pactouser T   H | T phase to user power | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactouser T   H"}'` |
| PactouserTotal H | Total AC power to user | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PactouserTotal H"}'` |
| Pac to grid R  H | AC Side to Grid Power R | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pac to grid R  H"}'` |
| Pactogrid S  H | AC Side to Grid Power S | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactogrid S  H"}'` |
| Pactogrid T H | AC Side to Grid Power T | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactogrid T H"}'` |
| Pactogrid total H | Total AC Side to Grid Power | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pactogrid total H"}'` |
| PLocalLoad total H | Inverter power to local load total | load | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PLocalLoad total H"}'` |
| EPS Pac_R H | Off-grid R phase output power | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Pac_R H"}'` |
| EPS ActPac_R H | Off-grid R phase output active power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS ActPac_R H"}'` |
| EPS Pac_S H | Off-grid S phase output power | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Pac_S H"}'` |
| EPS ActPac_S H | Off-grid S phase output active power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS ActPac_S H"}'` |
| EPS Pac_T H | Off-grid T-phase output power | power | uint32; 0.1VA | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS Pac_T H"}'` |
| EPS ActPac_T H | Off-grid T-phase output active power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPS ActPac_T H"}'` |
| Loadpercent | Off-grid Output Load Percentage | power | uint16; 0.01 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Loadpercent"}'` |
| PSystem H | System Power Generation | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PSystem H"}'` |
| PSelf H | Spontaneous self-consumption power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"PSelf H"}'` |
| Pdischarge H | Discharge power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pdischarge H"}'` |
| Pcharge H | Charging power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Pcharge H"}'` |
| AC charge Power_H | AC charging power | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"AC charge Power_H"}'` |
| Extra AC Power to  grid_H | Additional inverter AC power to grid H | power | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Extra AC Power to  grid_H"}'` |
| udGEN_ApparentP_R   H | Diesel generator R apparent power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ApparentP_R   H"}'` |
| udGEN_ApparentP_S   H | Diesel generator S-phase apparent power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ApparentP_S   H"}'` |
| udGEN_ApparentP_T   H | Diesel generator T-phase apparent power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ApparentP_T   H"}'` |
| udGEN_ActiveP_R   H | Diesel Generator R Phase Active Power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ActiveP_R   H"}'` |
| udGEN_ActiveP_S   H | Diesel Generator S Phase Active Power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ActiveP_S   H"}'` |
| udGEN_ActiveP_T   H | Diesel Generator T-Phase Active Power | generator | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udGEN_ActiveP_T   H"}'` |
| Eactoday H | Daily Power Generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eactoday H"}'` |
| Eac total H | Total Power Generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eac total H"}'` |
| EPVAll_Today H | PV Daily Power Generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EPVAll_Today H"}'` |
| Epv_total H | PV Total Energy | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Epv_total H"}'` |
| EChargeToday H | Daily Charge | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EChargeToday H"}'` |
| EChargeTotal H | Total Charges | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EChargeTotal H"}'` |
| EDischargeToday H | Daily Discharge Volume | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EDischargeToday H"}'` |
| EDischargeTotal H | Total Discharge | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EDischargeTotal H"}'` |
| EACharge_Today_H | AC Daily Charge | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EACharge_Today_H"}'` |
| EACharge_Total_H | Total AC Charging | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EACharge_Total_H"}'` |
| Eextra_today H | External grid-connected inversion energy on the same day | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eextra_today H"}'` |
| Eextra_total H | Total external grid-connected inverter energy | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eextra_total H"}'` |
| Esystem_today H | System Daily Power Generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Esystem_today H"}'` |
| Esystem_total H | Total system power generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Esystem_total H"}'` |
| Eself_today H | Spontaneous Daily Electricity Generation | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eself_today H"}'` |
| Eself_total H | Total spontaneous self-consumption | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eself_total H"}'` |
| Eload_today H | Load Power Consumption Day | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eload_today H"}'` |
| Eload_total H | Total Power Consumed by Load | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"Eload_total H"}'` |
| EtoGrid_today H | Infeed Grid Battery Days | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EtoGrid_today H"}'` |
| EtoGrid_total H | Total power fed into the grid | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EtoGrid_total H"}'` |
| EfromGrid_today H | Grid Intake Day | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EfromGrid_today H"}'` |
| EfromGrid_total H | Total grid intake | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"EfromGrid_total H"}'` |
| dEpvToGridTodayEE | Electricity on the Internet (electricity sold) day | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dEpvToGridTodayEE","domain":"energy","description":"Electricity on the Internet (electricity sold) day"}'` |
| dEpvToGridTotalEE | Total on-line electricity (electricity sold) | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dEpvToGridTotalEE","domain":"energy","description":"Total on-line electricity (electricity sold)"}'` |
| dEGridToLoadTodayEE | Date of Purchase of Electricity (Buy Electricity) | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dEGridToLoadTodayEE","domain":"energy","description":"Date of Purchase of Electricity (Buy Electricity)"}'` |
| dEGridToLoadTotalEE | Total Electricity Purchased (Bought) | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dEGridToLoadTotalEE","domain":"energy","description":"Total Electricity Purchased (Bought)"}'` |
| dESelfToLoadTodayEE | Self-Sufficiency Battery Day | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dESelfToLoadTodayEE","domain":"energy","description":"Self-Sufficiency Battery Day"}'` |
| dESelfToLoadTotalEE | Total Self-Sufficiency Battery | energy | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"dESelfToLoadTotalEE","domain":"energy","description":"Total Self-Sufficiency Battery"}'` |
| uwParallelType | Parallel Type | parallel | uint16 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"uwParallelType"}'` |
| HostSerialNum5 | Host serial number 9 ~ 10 characters, indicating the year of machine production | parallel | ascii | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"HostSerialNum5"}'` |
| HostSerialNum6 | Host serial number 11-12 characters, indicating the week in which the machine was manufactured | parallel | ascii | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"HostSerialNum6"}'` |
| HostSerialNum7 | Host Serial Number 13-14 characters | parallel | ascii | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"HostSerialNum7"}'` |
| HostSerialNum8 | Host Serial Number 15-16 characters | parallel | ascii | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"HostSerialNum8"}'` |
| ubParallelDeviceID | Parallel Device ID | parallel | uint8 | boot_identity | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"ubParallelDeviceID"}'` |
| udwParallelPVPower-H | Total Parallel PV Power | parallel | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallelPVPower-H"}'` |
| sdwParallelGridPower-H | Total power of parallel electromechanical grids | parallel | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"sdwParallelGridPower-H"}'` |
| udwParallelLoadPower-H | Total Parallel Load Power | parallel | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallelLoadPower-H"}'` |
| sdwParallelBatPower-H | Total Parallel Battery Power | parallel | sint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"sdwParallelBatPower-H"}'` |
| udwParallelSelfPower-H | Parallel machine self-consumption power | parallel | uint32; 0.1W | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallelSelfPower-H"}'` |
| udwParallel_EPVToady_H | Parallel PV Daily Power Generation | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EPVToady_H"}'` |
| udwParallel_EPVTotal_H | Shutdown PV Total Energy | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EPVTotal_H"}'` |
| udwParallel_ESelfToday_H | Parallel machine spontaneous daily power generation | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ESelfToday_H"}'` |
| udwParallel_ESelfTotal_H | Total amount of electricity generated by the parallel machine itself | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ESelfTotal_H"}'` |
| udwParallel_ELoadToday_H | Parallel Load Power Consumption Day | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ELoadToday_H","domain":"parallel","description":"Parallel Load Power Consumption Day"}'` |
| udwParallel_ELoadToday_H | Total power consumption of parallel loads | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ELoadToday_H","domain":"parallel","description":"Total power consumption of parallel loads"}'` |
| udwParallel_EPVtoGridToday_H | On-line electricity consumption (electricity sold) days | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EPVtoGridToday_H"}'` |
| udwParallel_EPVtoGridTotal_H | Total on-line electricity (electricity sold) | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EPVtoGridTotal_H"}'` |
| udwParallel_EGridtoLoadToday_H | Parallel Purchase of Electricity (Purchase of Electricity) Day | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EGridtoLoadToday_H"}'` |
| udwParallel_EGridtoLoadTotal_H | Total amount of electricity purchased (purchased) in parallel machines | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EGridtoLoadTotal_H"}'` |
| udwParallel_ESelftoLoadToday_H | Parallel Self-Sufficiency Battery Day | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ESelftoLoadToday_H"}'` |
| udwParallel_ESelftoLoadTotal_H | Parallel Self-Sufficiency Total Power | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_ESelftoLoadTotal_H"}'` |
| SOC | Battery SOC | parallel | uint16; 0.01 | live_flow | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"SOC","domain":"parallel"}'` |
| udwParallel_EBatChrToday_H | Parallel Battery Charge Battery Day | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EBatChrToday_H"}'` |
| udwParallel_EBatChrTotal_H | Total charge amount of parallel battery | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EBatChrTotal_H"}'` |
| udwParallel_EBatDisChrToday_H | Parallel battery discharge amount day | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EBatDisChrToday_H"}'` |
| udwParallel_EBatDisChrTotal_H | Total discharge capacity of parallel battery | parallel | uint32; 0.1kWh | slow_counter | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"get","category":"input_data","name":"udwParallel_EBatDisChrTotal_H"}'` |

### SETTERS - write settings or commands to the inverter (201)

These commands reach the common setter only after MQTT-origin validation. The ESP resolves the name, converts the application value, reads the current inverter value internally, checks operating state and interlocks, writes the complete value, and verifies it by read-back. Never retry a `service_only` command automatically.

| Setter guard | Count | Required behavior |
|---|---:|---|
| hot_edit_confirmed | 2 | Confirmed internal read-before-write and read-back. |
| hot_edit_guarded | 51 | Also requires guarded_interlock and validated dependencies. |
| runtime_command | 2 | Fresh valid runtime state; transition/status monitoring. |
| standby_required | 47 | Fresh standby or stopped state. |
| commissioning_only | 71 | Fresh standby/stopped state and commissioning_interlock. |
| service_only | 28 | Fresh standby/stopped state and service_interlock; never auto-retry. |

| Setting | Description | Domain | Application value type/unit | Guard | Documented meaning | Mosquitto setter command template |
|---|---|---|---|---|---|---|
| OnOffSet | Switch on/off | system | uint16 | runtime_command | On: 1; Off: 0; Restart: 0xA5 (TBD) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OnOffSet","value":"<new-value>","confirmed":true}'` |
| DeviceID | Device Correspondence Address | communications | uint16 | commissioning_only | Communication ID | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DeviceID","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BaudrateSet | Select Baud Rate | communications | uint16 | commissioning_only | 0:9600bps; 1:38400bps | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BaudrateSet","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Serial NO.1 | Serial number 1 | identity | ascii | service_only | Product serial number, valid minimum 16 digits | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.1","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.2 | Serial number 2 | identity | ascii | service_only | Serial number 2 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.2","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.3 | Serial number 3 | identity | ascii | service_only | Serial number 3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.3","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.4 | Serial numbe 4 | identity | ascii | service_only | Serial numbe 4 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.4","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.5 | Serial number 5 | identity | ascii | service_only | Serial number 5 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.5","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.6 | Serial number 6 | identity | ascii | service_only | Serial number 6 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.6","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.7 | Serial numbe 7 | identity | ascii | service_only | Serial numbe 7 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.7","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.8 | Serial numbe 8 | identity | ascii | service_only | Serial numbe 8 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.8","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.9 | Serial number 9 | identity | ascii | service_only | Serial number 9 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.9","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.10 | Serial number 10 | identity | ascii | service_only | Serial number 10 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.10","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.11 | Serial number 11 | identity | ascii | service_only | Serial number 11 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.11","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.12 | Serial number 12 | identity | ascii | service_only | Serial number 12 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.12","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.13 | Serial number 13 | identity | ascii | service_only | Serial number 13 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.13","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.14 | Serial number 14 | identity | ascii | service_only | Serial number 14 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.14","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Serial NO.15 | Serial numbe 15 | identity | ascii | service_only | Serial numbe 15 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Serial NO.15","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Rs485WorkMode | 485 Operating Mode Selection | communications | uint16 | commissioning_only | 0: Slave mode (ate); 1: Host mode (BMS485); 2: Host mode (Meter485) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Rs485WorkMode","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Year | System Time - Year | system | uint16; Year | hot_edit_guarded | System Time - Year | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Year","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Month | System Time - Month | system | uint16; Month | hot_edit_guarded | System Time - Month | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Month","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Day | System Time - Day | system | uint16; Day | hot_edit_guarded | System Time - Day | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Day","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Hour | System Time - Hours | system | uint16; Hour | hot_edit_guarded | System Time - Hours | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Hour","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Minute | System Time - Minutes | system | uint16; Minutes | hot_edit_guarded | System Time - Minutes | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Minute","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Second | System Time - Seconds | system | uint16; Seconds | hot_edit_guarded | Set time to seconds to take effect | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Second","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Weekday | System Week | system | uint16 | hot_edit_guarded | System Week | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Weekday","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Manufacturer Info 1 | Manufacturer Info 1 | identity | ascii_or_uint16 | service_only | Optional vendor name when configuring | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Manufacturer Info 1","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Module 4 | Inverter Mode (4) | identity | uint16 | service_only | Inverter Mode (4) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Module 4","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Module 3 | Inverter Mode (3) | identity | uint16 | service_only | Inverter Mode (3) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Module 3","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Module 2 | Inverter Mode (2) | identity | uint16 | service_only | Inverter Mode (2) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Module 2","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Module 1 | Inverter Mode (1) | identity | uint16 | service_only | Inverter Mode (1) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Module 1","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Reset to factory | Factory reset | system | uint16 | service_only | 0x01: Restore user data; 0xA0: Restore factory settings | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Reset to factory","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| UpdateFileType | Upgrade File Type | system | uint16 | service_only | 0x01: bin upgrade; 0x10: Hex upgrade | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"UpdateFileType","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| ubScreenType | Screen Type | system | uint8 | commissioning_only | 0: Small screen; 1: Large screen | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubScreenType","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| SafetyId | Safety regulation number | grid | uint16 | commissioning_only | Safety regulation number | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SafetyId","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ReactivePowerDelayTIme | Reactive Percent Response Time | grid | uint16; 20ms | commissioning_only | Reactive Percent Response Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ReactivePowerDelayTIme","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ActiveOverloadEnable | Active Power Overload Enabled | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ActiveOverloadEnable","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| AcChargePowerRate | Inverter Max Take-Off Power Percentage | grid | uint16; 1%Pn | commissioning_only | Grid Intake | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcChargePowerRate","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ActivePowerSlope | Active power rate of change (N4105) | grid | uint16; 0.1Pn/min | commissioning_only | 0: Not Enabled; Other: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ActivePowerSlope","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ActivePowerRate | Inverter Max Output Active Power Percentage | grid | uint16; 1%Pn | commissioning_only | Power Generation to the Grid | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ActivePowerRate","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ReactivePowerRate | Inverter Max Output Reactive Power Percentage | grid | sint16; 1%Pn | commissioning_only | Inverter Max Output Reactive Power Percentage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ReactivePowerRate","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| PowerFactorSet | Inverter output power factor 10,000 times | grid | uint16 | commissioning_only | PF=(X-10000)/10000 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PowerFactorSet","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| PVModelSelect | MPPT Mode | grid | uint16 | commissioning_only | 0: Independent MPPT mode; 1: DC source mode; 2: Parallel MPPT mode | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PVModelSelect","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| PvStartVoltage | PV start voltage | grid | uint16; 0.1V | commissioning_only | PV start voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PvStartVoltage","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| FastDerating_en | Fast Down Enable | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastDerating_en","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Island_en | Island Enabling | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Island_en","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| VFRT_en | High and low penetration enable | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"VFRT_en","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| DRMS_en | DRMS Enabled | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DRMS_en","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| NLineConnectMode | N-line enable | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NLineConnectMode","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| NToGNDDetect | Zero Earth Detection | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NToGNDDetect","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| ZeroPowerOutputEnable | Zero power output enabled | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ZeroPowerOutputEnable","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| FastMpptEnable | Fast mppt enable | grid | uint16 | commissioning_only | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastMpptEnable","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| CtRatioSet | CT Rheological Ratio Setting | grid | uint16 | commissioning_only | Reserved | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"CtRatioSet","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| CTMode | Using CT Mode | grid | uint16 | commissioning_only | 0:METER; 1:CT | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"CTMode","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| LocalAntiBackflowEnable | Anti-reflux enable | grid | uint16 | commissioning_only | 0: Disable; 1: Total anti-reflux enable; 2: Three-phase independent anti-reflux enable | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalAntiBackflowEnable","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BackflowMeterPowerLimit_R | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | commissioning_only | Anti-reflux Reflux Power Limit | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BackflowMeterPowerLimit_R","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BackflowMeterPowerLimit_S | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | commissioning_only | Anti-reflux Reflux Power Limit | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BackflowMeterPowerLimit_S","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BackflowMeterPowerLimit_T | Anti-reflux Reflux Power Limit | grid | sint16; 1Pn% | commissioning_only | Anti-reflux Reflux Power Limit | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BackflowMeterPowerLimit_T","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BackflowFaultTime | Anti-reflux failure time | grid | uint16; 1s | commissioning_only | Anti-reflux failure time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BackflowFaultTime","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BackflowFaultPowerRate | Percentage of active anti-reflux failure | grid | sint16; 1Pn% | commissioning_only | Machine operates at given power after anti-reverse flow failure | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BackflowFaultPowerRate","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Power on config falg | Boot navigation flags | system | uint16 | commissioning_only | 0x00: default; 0x01: configuring (no storage, shutdown); 0x55: configuration complete (save) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Power on config falg","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Vac low C | Grid Low Voltage Restrictions Connected to the Grid | grid | uint16; 0.1V | commissioning_only | Grid-connected conditions | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Vac low C","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Vac high C | Grid High Voltage Restriction Connected to Grid | grid | uint16; 0.1V | commissioning_only | Grid High Voltage Restriction Connected to Grid | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Vac high C","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Fac low C | Grid Low Frequency Restrictions Connected to the Grid | grid | uint16; 0.01Hz | commissioning_only | Grid Low Frequency Restrictions Connected to the Grid | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Fac low C","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Fac high C | Grid High Frequency Restrictions Connected to the Grid | grid | uint16; 0.01Hz | commissioning_only | Grid High Frequency Restrictions Connected to the Grid | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Fac high C","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| NominalEPSVolt | Optional rated off-grid voltage | eps | uint16 | standby_required | 0: 230V; 1: 240V; 2: 208V; 3: 220V (off-grid); 4: 200V (off-grid) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":"<new-value>","confirmed":true}'` |
| NominalEPSFre | Optional rated off-grid frequency | eps | uint16 | standby_required | 0:50Hz; 1:60Hz | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSFre","value":"<new-value>","confirmed":true}'` |
| EPS_En | Off-grid functionality enabled | eps | uint16 | standby_required | Off-grid functionality enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EPS_En","value":"<new-value>","confirmed":true}'` |
| Bypass_En | Bypass mode enabled | eps | uint16 | standby_required | Bypass mode enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Bypass_En","value":"<new-value>","confirmed":true}'` |
| UPS_En | UPS Function Enabled | eps | uint16 | standby_required | TBC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"UPS_En","value":"<new-value>","confirmed":true}'` |
| ubN_PE_RelayCMD | Zero earth relay control enabled | eps | uint16 | commissioning_only | Attendance Functional Requirements | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubN_PE_RelayCMD","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| unAFCICtrlReg | AFCI Self-Test Command | system | uint16 | service_only | Bit0: SelfCheckCmd, ; Bit1: ClrFaultCmd, ; Bit2~7:reserved,  | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"unAFCICtrlReg","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| uwAFCIThresholdValue | AFCI error threshold | system | uint16 | service_only | AFCI error threshold | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwAFCIThresholdValue","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| AFCIClrFaultCmd | AFCI Error Clear Command | system | uint16 | service_only | AFCI Error Clear Command | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AFCIClrFaultCmd","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| uwRestartVoltL | Fault reconnect grid low voltage | grid | uint16; 0.1V | commissioning_only | Fault reconnection and grid connection conditions | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwRestartVoltL","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| uwRestartVoltH | Fault reconnect grid high voltage | grid | uint16; 0.1V | commissioning_only | Fault reconnect grid high voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwRestartVoltH","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| uwRestartFreqL | Fault Reconnect Grid Low Frequency | grid | uint16; 0.01Hz | commissioning_only | Fault Reconnect Grid Low Frequency | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwRestartFreqL","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| uwRestartFreqH | Fault Reconnect Grid High Frequency | grid | uint16; 0.01Hz | commissioning_only | Fault Reconnect Grid High Frequency | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwRestartFreqH","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| BatteryType | Battery type | battery | uint16 | standby_required | 0: Lead acid; 1: Lithium battery; 2: Lithium battery without communication (off-grid); 3: User 2 (off-grid); 4: User 3 (off-grid); 5: NO Bat | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":"<new-value>","confirmed":true}'` |
| BatteryCompanySet | Battery communication protocol selection | bms | uint16 | standby_required | 0: null; 1: Protocol 1; 2: Protocol 2; 3: Protocol 3; 4: Protocol 4 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":"<new-value>","confirmed":true}'` |
| BatMdlSerialNum | Number of batteries in series - high-voltage batteries; | battery | uint16 | standby_required | Lead-acid batteries (for high-voltage battery systems) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatMdlSerialNum","value":"<new-value>","confirmed":true}'` |
| BatMdlParallNum | Number of battery parallel joints; | battery | uint16 | standby_required | Lead-acid batteries (reserved, unused) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatMdlParallNum","value":"<new-value>","confirmed":true}'` |
| ChargeCurrentLimit | Charge Limit Current | battery | uint16; 0.01A | standby_required | When the charging current needs to be lower than this value, enter the floating charging CC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeCurrentLimit","value":"<new-value>","confirmed":true}'` |
| VbatStopForDischarge | Discharge cut-off voltage | battery | uint16; 0.01V | standby_required | Discharge cut-off voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"VbatStopForDischarge","value":"<new-value>","confirmed":true}'` |
| Vbat constant charge | Charge cut-off voltage | battery | uint16; 0.01V | standby_required | CV voltage (lead-acid battery) | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Vbat constant charge","value":"<new-value>","confirmed":true}'` |
| ubHighVoltBatRatedVolt | Battery nominal voltage | battery | uint16; 1V | standby_required | Battery nominal voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubHighVoltBatRatedVolt","value":"<new-value>","confirmed":true}'` |
| ChargeRate | Charging power | battery | uint16; 0.01 | standby_required | Charging power | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeRate","domain":"battery","description":"Charging power","value":"<new-value>","confirmed":true}'` |
| ChargeRate | Discharge power | battery | uint16; 0.01 | standby_required | Discharge power | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeRate","domain":"battery","description":"Discharge power","value":"<new-value>","confirmed":true}'` |
| BatFirstStopSOC | Charge cut-off SOC | battery | uint16; 0.01 | standby_required | Charge cut-off SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatFirstStopSOC","value":"<new-value>","confirmed":true}'` |
| OnLineStopSOC | Grid-connected discharge cut-off SOC | battery | uint16; 0.01 | standby_required | Grid-connected discharge cut-off SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OnLineStopSOC","value":"<new-value>","confirmed":true}'` |
| OffLineStopSoc | Off-grid Discharge Cutoff SOC | battery | uint16; 0.01 | standby_required | Off-grid Discharge Cutoff SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OffLineStopSoc","value":"<new-value>","confirmed":true}'` |
| Gen Charge En | Total Diesel Charging Enabled | system | uint16 | hot_edit_guarded | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Charge En","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| AcCharge_En | AC Charge Enabled | system | uint16 | hot_edit_guarded | 0: Disabled; 1: Enabled | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcCharge_En","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| LoadFirstDischargeDisableFlag | Load Preferred Discharge Prohibit Sign | system | uint16 | hot_edit_guarded | TBC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LoadFirstDischargeDisableFlag","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| RemotePowerControl | One-click charging and discharging | system | uint16 | runtime_command | 255: Fail; 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RemotePowerControl","value":"<new-value>","confirmed":true}'` |
| NumberTimePeriods | Number of charging and discharging periods - number of currently valid periods, starting with period 1 | system | uint16 | hot_edit_guarded | 0 ~ 10: Number of time periods; 0xA5: Number of time periods reset, all time period setting values reset | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NumberTimePeriods","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart1 | Period 1 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart1","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd1 | Period 1 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd1","domain":"system","description":"Period 1 End Time","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate1 | Period 1 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 1 Priority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart2 | Period 2 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart2","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd2 | Period 2 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd2","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate2 | Period 2 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate2","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart3 | Period 3 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart3","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd3 | Slot 3 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd3","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate3 | Period 3 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate3","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart4 | Period 4 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart4","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd4 | Period 4 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd4","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate4 | Period 4 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate4","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart5 | Period 5 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart5","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd5 | Slot 5 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd5","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate5 | Period 5 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate5","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart6 | Period 6 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart6","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd1 | Slot 6 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd1","domain":"system","description":"Slot 6 End Time","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate1 | Period 6 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 6 Priority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart7 | Period 7 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart7","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd1 | Segment 7 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd1","domain":"system","description":"Segment 7 End Time","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate1 | Period 7 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 7 Priority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart8 | Period 8 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart8","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd1 | Slot 8 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd1","domain":"system","description":"Slot 8 End Time","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate1 | Period 8 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 8 Priority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart9 | Period 9 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart9","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd1 | Period 9 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd1","domain":"system","description":"Period 9 End Time","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate1 | Period 9 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 9 Priority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeStart10 | Period 10 Start Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeStart10","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeEnd10 | Slot 10 End Time | system | uint16; 1min | hot_edit_guarded | H: hour 0 ~ 23; L: minute 0 ~ 59 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeEnd10","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| PeriodTimeRate10 | Period 10 Priority | system | uint16; 1Pn% | hot_edit_guarded | 0: Load first; 1: Battery first; 2: Grid first | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate10","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| ChargeSourcePriority | Charge Priority | system | uint8 | hot_edit_guarded | 0:CSO; 1:SNU; 2:OSO | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeSourcePriority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| SourcePriority | Energy Priority | system | uint8 | hot_edit_guarded | 0: Sol; 1: UTI; 2: SBU; 10: Grid connected output mode | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SourcePriority","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| uwCC_DisChrLead_100T | Lead Acid Discharge Current | battery | uint16; 0.01A | standby_required | Lead Acid Discharge Current | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwCC_DisChrLead_100T","value":"<new-value>","confirmed":true}'` |
| ubOP_RecoverDischargeSOC | Grid-connected stop discharge recovery SOC | battery | uint8; 0.01 | standby_required | Grid-connected stop discharge recovery SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubOP_RecoverDischargeSOC","value":"<new-value>","confirmed":true}'` |
| ubOffGrid_RecoverDischargeSOC | Off-grid Stop Discharge Recovery SOC | battery | uint8; 0.01 | standby_required | Off-grid Stop Discharge Recovery SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubOffGrid_RecoverDischargeSOC","value":"<new-value>","confirmed":true}'` |
| BatUnderVol | Battery Low Voltage Shutdown Voltage | battery | uint16; 0.1V | standby_required | Battery Low Voltage Shutdown Voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatUnderVol","value":"<new-value>","confirmed":true}'` |
| AcChargingCurrent | AC Charge Limit Current | battery | uint16; 0.1A | standby_required | AC Charge Limit Current | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcChargingCurrent","value":"<new-value>","confirmed":true}'` |
| uwFloatV_Lead_100T | Floating Charge Voltage | battery | uint16; 0.01V | standby_required | Floating Charge Voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwFloatV_Lead_100T","value":"<new-value>","confirmed":true}'` |
| BAT2AC_Volt | Battery-to-market voltage | battery | uint16; 0.1V | standby_required | Battery-to-market voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BAT2AC_Volt","value":"<new-value>","confirmed":true}'` |
| AC2BAT_Volt | Mains to Battery Voltage | battery | uint16; 0.1V | standby_required | Mains to Battery Voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AC2BAT_Volt","value":"<new-value>","confirmed":true}'` |
| ubLeadAcid_BatSubType | Subtype of lead-acid battery | battery | uint8 | standby_required | 0:USE; 1:SLD; 2:FLD; 3:GEL; 4:L14; 5:L15; 6:L16; 7:N13; 8:N14; 9:LIT | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":"<new-value>","confirmed":true}'` |
| IncreChar_MaxTim | Improved charging time | battery | uint16; 1min | standby_required | Improved charging time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"IncreChar_MaxTim","value":"<new-value>","confirmed":true}'` |
| BatUnderVolt_Point | Battery undervoltage alarm point | battery | uint16; 0.1V | standby_required | Battery undervoltage alarm point | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatUnderVolt_Point","value":"<new-value>","confirmed":true}'` |
| Equalization | Equalization Mode Enabled | battery | uint8 | standby_required | 0: Disable; 1: Enable | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Equalization","value":"<new-value>","confirmed":true}'` |
| EQBatteryTime | Equalize charging time | battery | uint16; 1min | standby_required | Equalize charging time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EQBatteryTime","value":"<new-value>","confirmed":true}'` |
| EQBatteryTimeout | Equalize Charge Delay Time | battery | uint16; 1min | standby_required | Equalize Charge Delay Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EQBatteryTimeout","value":"<new-value>","confirmed":true}'` |
| EqualizationCycle | Balanced charge interval | battery | uint8; 1DAY | standby_required | Balanced charge interval | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EqualizationCycle","value":"<new-value>","confirmed":true}'` |
| EqualizationImmediately | Turn on Balanced Charging now | battery | uint16 | service_only | 0: Disable; 1: Enable | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EqualizationImmediately","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| flcdEn | LCD setting enable bit | system | uint16 | hot_edit_guarded | 0: ECOMode_En (Energy Saving Mode Enabled); 1: OverLoad_RestartEn; 2: OverTemp_RestartEn; 3: InputChange_RemEn; 4: OPSplit_PhaseEn; 5: Generator_AutoIPEn; 6: DualChannel_LoadEn; 7: ubridFeedBackEn; 8 ~ 15: unused | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| BatLVBreak_RestartVolt | Low Voltage Disconnect Battery Recovery Point | battery | uint16; 0.1V | standby_required | Low Voltage Disconnect Battery Recovery Point | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatLVBreak_RestartVolt","value":"<new-value>","confirmed":true}'` |
| BatNeedChr_Volt | Battery Recharge Recovery Point | battery | uint16; 0.1V | standby_required | Battery Recharge Recovery Point | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatNeedChr_Volt","value":"<new-value>","confirmed":true}'` |
| NonCriticlLoad_BatDisConVolt | Non-critical load disconnect battery voltage | battery | uint16; 0.1V | standby_required | Non-critical load disconnect battery voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NonCriticlLoad_BatDisConVolt","value":"<new-value>","confirmed":true}'` |
| BatHighVolt_DisConPoint | Overvoltage disconnection voltage | battery | uint16; 0.1V | standby_required | Overvoltage disconnection voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatHighVolt_DisConPoint","value":"<new-value>","confirmed":true}'` |
| BatOverDisCharge_Delay | Battery Overdischarge Delay Time | battery | uint16; 1s | standby_required | Battery Overdischarge Delay Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatOverDisCharge_Delay","value":"<new-value>","confirmed":true}'` |
| DspBeepOnOff | Buzzer switch | system | uint8 | hot_edit_confirmed | 0: Disable; 1: Enable | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DspBeepOnOff","value":"<new-value>","confirmed":true}'` |
| OverloadToBypass | Overload transfer bypass enable | eps | uint8 | hot_edit_guarded | 0: Disable; 1: Enable | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OverloadToBypass","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| AcInputType | Off-grid output mode | grid | uint8 | standby_required | 0:APL; 1:UPS | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcInputType","value":"<new-value>","confirmed":true}'` |
| EQBatteryVoltage_100T | Balanced Charging Voltage | battery | uint16 | standby_required | Balanced Charging Voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EQBatteryVoltage_100T","value":"<new-value>","confirmed":true}'` |
| Parallel_Mode | Parallel Mode | parallel | uint8 | standby_required | 0:SIG; 1:PAL; 2:3P1; 3:3P2; 4:3P3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":"<new-value>","confirmed":true}'` |
| ubParallelDeviveID | Parallel can communication address | parallel | uint8 | standby_required | Parallel can communication address | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubParallelDeviveID","value":"<new-value>","confirmed":true}'` |
| ubParallelDeviveType | Parallel Device Type | parallel | uint8 | standby_required | 0: Host; 1: Slave | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubParallelDeviveType","value":"<new-value>","confirmed":true}'` |
| ubBMSWorkMode | BMS communication method | bms | uint8 | standby_required | 0: Disable; 1: can communication; 2: 485 communication | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBMSWorkMode","value":"<new-value>","confirmed":true}'` |
| uwGridPowerCompensation | Grid Power Compensation | grid | uint16 | hot_edit_guarded | Grid Power Compensation | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"uwGridPowerCompensation","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| Gen Port Work Mode | Diesel Port Function Selection | generator | uint16 | standby_required | 0.Default; 1.Generator En; 2.Gen Force; 3.SmartLoad Output; 4.On Grid always on; 5.Off Grid immediately off; 6.AC Couple on SecEPS side | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Port Work Mode","value":"<new-value>","confirmed":true}'` |
| Gen Charge Curr Limit | Diesel Charging Current Limit | generator | uint16; 1A | standby_required | Diesel Charging Current Limit | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Charge Curr Limit","value":"<new-value>","confirmed":true}'` |
| Gen Input Rated Power | Generator Input Rated Power | generator | uint16; 10W | standby_required | Generator Input Rated Power | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Input Rated Power","value":"<new-value>","confirmed":true}'` |
| SecEPS ON SOC/Vbat | (Lithium) Start SOC | generator | uint16; 0.01 | hot_edit_guarded | (Lithium) Start SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SecEPS ON SOC/Vbat","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| SecEPS ON Vbat | (Lead acid) Starting battery voltage | generator | uint16; 0.1V | hot_edit_guarded | (Lead acid) Starting battery voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SecEPS ON Vbat","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| SecEPS OFF SOC/Vbat | (Lithium) Shutdown SOC | generator | uint16; 0.01 | hot_edit_guarded | (Lithium) Shutdown SOC | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SecEPS OFF SOC/Vbat","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| SecEPS OFF Vbat | (Lead Acid) Shutdown Battery Voltage | generator | uint16; 0.1V | hot_edit_guarded | (Lead Acid) Shutdown Battery Voltage | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SecEPS OFF Vbat","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| SecEPS  On PV Power Min | Minimum power of photovoltaic startup smart load | generator | uint16; 10W | hot_edit_guarded | Minimum power of photovoltaic startup smart load | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SecEPS  On PV Power Min","value":"<new-value>","confirmed":true,"guarded_interlock":true}'` |
| ubBluetoothEn | Bluetooth Enabled | system | uint8 | hot_edit_confirmed | 1: Bluetooth On; 0: Bluetooth Off | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBluetoothEn","value":"<new-value>","confirmed":true}'` |
| upgrade notification | Data update notification, wifi re-submit 0304 data to the server | communications | uint8 | service_only | 0: Invalid default; 1: Trigger update | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"upgrade notification","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| Datalogger Restart | wifi factory settings restored, server domain name and escalation time modified | communications | uint8 | service_only | 0: Invalid by default; 1: Trigger a factory reset | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Datalogger Restart","value":"<new-value>","confirmed":true,"service_interlock":true}'` |
| LocalSafetyCmd | User Safety Selection Instructions | grid | uint16 | commissioning_only | TBD; 0: Regional Standard Safety Code; 1: Subscriber Wide Range; 2: Grid Company Safety Code | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalSafetyCmd","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltLow1EE | Low Grid Voltage Protection Tier 1 | grid | uint16; 0.1V | commissioning_only | Low Grid Voltage Protection Tier 1 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltLow1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltHigh1EE | High Grid Voltage Protection Tier 1 | grid | uint16; 0.1V | commissioning_only | High Grid Voltage Protection Tier 1 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltHigh1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqLow1EE | Grid Low Frequency Protection First Order | grid | uint16; 0.01Hz | commissioning_only | Grid Low Frequency Protection First Order | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqLow1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqHigh1EE | Grid High Frequency Protection First Order | grid | uint16; 0.01Hz | commissioning_only | Grid High Frequency Protection First Order | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqHigh1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltLow2EE | Low Grid Voltage Protection Level 2 | grid | uint16; 0.1V | commissioning_only | Low Grid Voltage Protection Level 2 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltLow2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltHigh2EE | High Grid Voltage Protection Level 2 | grid | uint16; 0.1V | commissioning_only | High Grid Voltage Protection Level 2 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltHigh2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqLow2EE | Grid Low Frequency Protection Second Order | grid | uint16; 0.01Hz | commissioning_only | Grid Low Frequency Protection Second Order | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqLow2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqHigh2EE | Grid High Frequency Protection Second Order | grid | uint16; 0.01Hz | commissioning_only | Grid High Frequency Protection Second Order | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqHigh2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltLow3EE | Low Grid Voltage Protection Level 3 | grid | uint16; 0.1V | commissioning_only | Low Grid Voltage Protection Level 3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltLow3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fGridVoltHigh3EE | High Grid Voltage Protection Level 3 | grid | uint16; 0.1V | commissioning_only | High Grid Voltage Protection Level 3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fGridVoltHigh3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqLow3EE | Grid Low Frequency Protection Level 3 | grid | uint16; 0.01Hz | commissioning_only | Grid Low Frequency Protection Level 3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqLow3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| fFreqHigh3EE | Grid High Frequency Protection Level 3 | grid | uint16; 0.01Hz | commissioning_only | Grid High Frequency Protection Level 3 | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"fFreqHigh3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVLowCutTime1EE | Low Grid Voltage First Order Protection Time | grid | uint16; 20ms | commissioning_only | Low Grid Voltage First Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVLowCutTime1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVHighCutTime1EE | High grid voltage first-order protection time | grid | uint16; 20ms | commissioning_only | High grid voltage first-order protection time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVHighCutTime1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFLowCutTime1EE | Low Grid Frequency First Order Protection Time | grid | uint16; 20ms | commissioning_only | Low Grid Frequency First Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFLowCutTime1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFHighCutTime1EE | High Grid Frequency First Order Protection Time | grid | uint16; 20ms | commissioning_only | High Grid Frequency First Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFHighCutTime1EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVLowCutTime2EE | Low Grid Voltage Second Order Protection Time | grid | uint16; 20ms | commissioning_only | Low Grid Voltage Second Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVLowCutTime2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVHighCutTime2EE | High Grid Voltage Second Order Protection Time | grid | uint16; 20ms | commissioning_only | High Grid Voltage Second Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVHighCutTime2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFLowCutTime2EE | Low Grid Frequency Second Order Protection Time | grid | uint16; 20ms | commissioning_only | Low Grid Frequency Second Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFLowCutTime2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFHighCutTime2EE | High Grid Frequency Second Order Protection Time | grid | uint16; 20ms | commissioning_only | High Grid Frequency Second Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFHighCutTime2EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVLowCutTime3EE | Low grid voltage third-order protection time | grid | uint16; 20ms | commissioning_only | Low grid voltage third-order protection time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVLowCutTime3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| wVHighCutTime3EE | High grid voltage third-order protection time | grid | uint16; 20ms | commissioning_only | High grid voltage third-order protection time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"wVHighCutTime3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFLowCutTime3EE | Low Grid Frequency Third Order Protection Time | grid | uint16; 20ms | commissioning_only | Low Grid Frequency Third Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFLowCutTime3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| udFHighCutTime3EE | High Grid Frequency Third Order Protection Time | grid | uint16; 20ms | commissioning_only | High Grid Frequency Third Order Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"udFHighCutTime3EE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| 10MinAVLimit | Voltage protection for ten minutes | grid | uint16; 0.1V | commissioning_only | Voltage protection for ten minutes | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"10MinAVLimit","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| U10minTime | 10 Minute Average Voltage Protection Time | grid | uint16; 20ms | commissioning_only | 10 Minute Average Voltage Protection Time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"U10minTime","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| Time start | Connection time | grid | uint16; 1s | commissioning_only | Connection time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Time start","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| RestartDelayTime | Reconnection time | grid | uint16; 1s | commissioning_only | Reconnection time | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RestartDelayTime","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| PowerStartSlope | Load rate | grid | uint16; 0.1Pn%/min | commissioning_only | Load rate | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PowerStartSlope","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |
| PowerRestartSlopeEE | Restart Load Rate | grid | uint16; 0.1Pn%/min | commissioning_only | Restart Load Rate | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PowerRestartSlopeEE","value":"<new-value>","confirmed":true,"commissioning_interlock":true}'` |

### Enumerated setter choices (51 settings)

Each row below is a complete command illustration for one documented choice. Use a fresh numeric `command_id` for every publication. Boolean enable/disable choices use `true` and `false`; other choices use their documented application mode number. These are requested settings, not Modbus addresses or raw register words.

#### BaudrateSet - Select Baud Rate

| Choice | Application value | Mosquitto command |
|---|---:|---|
| 9600bps | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BaudrateSet","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| 38400bps | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BaudrateSet","value":1,"confirmed":true,"commissioning_interlock":true}'` |

#### Rs485WorkMode - 485 Operating Mode Selection

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Slave mode (ate) | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Rs485WorkMode","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| Host mode (BMS485) | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Rs485WorkMode","value":1,"confirmed":true,"commissioning_interlock":true}'` |
| Host mode (Meter485) | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Rs485WorkMode","value":2,"confirmed":true,"commissioning_interlock":true}'` |

#### Reset to factory - Factory reset

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Restore user data | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Reset to factory","value":1,"confirmed":true,"service_interlock":true}'` |
| Restore factory settings | `160` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Reset to factory","value":160,"confirmed":true,"service_interlock":true}'` |

#### UpdateFileType - Upgrade File Type

| Choice | Application value | Mosquitto command |
|---|---:|---|
| bin upgrade | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"UpdateFileType","value":1,"confirmed":true,"service_interlock":true}'` |
| Hex upgrade | `16` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"UpdateFileType","value":16,"confirmed":true,"service_interlock":true}'` |

#### ubScreenType - Screen Type

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Small screen | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubScreenType","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| Large screen | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubScreenType","value":1,"confirmed":true,"commissioning_interlock":true}'` |

#### ActiveOverloadEnable - Active Power Overload Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ActiveOverloadEnable","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ActiveOverloadEnable","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### PVModelSelect - MPPT Mode

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Independent MPPT mode | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PVModelSelect","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| DC source mode | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PVModelSelect","value":1,"confirmed":true,"commissioning_interlock":true}'` |
| Parallel MPPT mode | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PVModelSelect","value":2,"confirmed":true,"commissioning_interlock":true}'` |

#### FastDerating_en - Fast Down Enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastDerating_en","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastDerating_en","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### Island_en - Island Enabling

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Island_en","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Island_en","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### VFRT_en - High and low penetration enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"VFRT_en","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"VFRT_en","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### DRMS_en - DRMS Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DRMS_en","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DRMS_en","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### NLineConnectMode - N-line enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NLineConnectMode","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NLineConnectMode","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### NToGNDDetect - Zero Earth Detection

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NToGNDDetect","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NToGNDDetect","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### ZeroPowerOutputEnable - Zero power output enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ZeroPowerOutputEnable","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ZeroPowerOutputEnable","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### FastMpptEnable - Fast mppt enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastMpptEnable","value":false,"confirmed":true,"commissioning_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"FastMpptEnable","value":true,"confirmed":true,"commissioning_interlock":true}'` |

#### CTMode - Using CT Mode

| Choice | Application value | Mosquitto command |
|---|---:|---|
| METER | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"CTMode","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| CT | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"CTMode","value":1,"confirmed":true,"commissioning_interlock":true}'` |

#### LocalAntiBackflowEnable - Anti-reflux enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalAntiBackflowEnable","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| Total anti-reflux enable | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalAntiBackflowEnable","value":1,"confirmed":true,"commissioning_interlock":true}'` |
| Three-phase independent anti-reflux enable | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalAntiBackflowEnable","value":2,"confirmed":true,"commissioning_interlock":true}'` |

#### Power on config falg - Boot navigation flags

| Choice | Application value | Mosquitto command |
|---|---:|---|
| default | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Power on config falg","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| configuring (no storage, shutdown) | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Power on config falg","value":1,"confirmed":true,"commissioning_interlock":true}'` |
| configuration complete (save) | `85` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Power on config falg","value":85,"confirmed":true,"commissioning_interlock":true}'` |

#### NominalEPSVolt - Optional rated off-grid voltage

| Choice | Application value | Mosquitto command |
|---|---:|---|
| 230V | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":0,"confirmed":true}'` |
| 240V | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":1,"confirmed":true}'` |
| 208V | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":2,"confirmed":true}'` |
| 220V (off-grid) | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":3,"confirmed":true}'` |
| 200V (off-grid) | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSVolt","value":4,"confirmed":true}'` |

#### NominalEPSFre - Optional rated off-grid frequency

| Choice | Application value | Mosquitto command |
|---|---:|---|
| 50Hz | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSFre","value":0,"confirmed":true}'` |
| 60Hz | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"NominalEPSFre","value":1,"confirmed":true}'` |

#### BatteryType - Battery type

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Lead acid | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":0,"confirmed":true}'` |
| Lithium battery | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":1,"confirmed":true}'` |
| Lithium battery without communication (off-grid) | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":2,"confirmed":true}'` |
| User 2 (off-grid) | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":3,"confirmed":true}'` |
| User 3 (off-grid) | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":4,"confirmed":true}'` |
| NO Bat | `5` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryType","value":5,"confirmed":true}'` |

#### BatteryCompanySet - Battery communication protocol selection

| Choice | Application value | Mosquitto command |
|---|---:|---|
| null | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":0,"confirmed":true}'` |
| Protocol 1 | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":1,"confirmed":true}'` |
| Protocol 2 | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":2,"confirmed":true}'` |
| Protocol 3 | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":3,"confirmed":true}'` |
| Protocol 4 | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"BatteryCompanySet","value":4,"confirmed":true}'` |

#### Gen Charge En - Total Diesel Charging Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Charge En","value":false,"confirmed":true,"guarded_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Gen Charge En","value":true,"confirmed":true,"guarded_interlock":true}'` |

#### AcCharge_En - AC Charge Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disabled | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcCharge_En","value":false,"confirmed":true,"guarded_interlock":true}'` |
| Enabled | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcCharge_En","value":true,"confirmed":true,"guarded_interlock":true}'` |

#### RemotePowerControl - One-click charging and discharging

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Fail | `255` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RemotePowerControl","value":255,"confirmed":true}'` |
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RemotePowerControl","value":0,"confirmed":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RemotePowerControl","value":1,"confirmed":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"RemotePowerControl","value":2,"confirmed":true}'` |

#### PeriodTimeRate1 - Period 1 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 1 Priority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 1 Priority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 1 Priority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate2 - Period 2 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate2","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate2","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate2","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate3 - Period 3 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate3","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate3","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate3","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate4 - Period 4 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate4","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate4","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate4","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate5 - Period 5 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate5","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate5","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate5","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate1 - Period 6 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 6 Priority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 6 Priority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 6 Priority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate1 - Period 7 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 7 Priority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 7 Priority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 7 Priority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate1 - Period 8 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 8 Priority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 8 Priority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 8 Priority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate1 - Period 9 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 9 Priority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 9 Priority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate1","domain":"system","description":"Period 9 Priority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### PeriodTimeRate10 - Period 10 Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Load first | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate10","value":0,"confirmed":true,"guarded_interlock":true}'` |
| Battery first | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate10","value":1,"confirmed":true,"guarded_interlock":true}'` |
| Grid first | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"PeriodTimeRate10","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### ChargeSourcePriority - Charge Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| CSO | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeSourcePriority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| SNU | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeSourcePriority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| OSO | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ChargeSourcePriority","value":2,"confirmed":true,"guarded_interlock":true}'` |

#### SourcePriority - Energy Priority

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Sol | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SourcePriority","value":0,"confirmed":true,"guarded_interlock":true}'` |
| UTI | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SourcePriority","value":1,"confirmed":true,"guarded_interlock":true}'` |
| SBU | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SourcePriority","value":2,"confirmed":true,"guarded_interlock":true}'` |
| Grid connected output mode | `10` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"SourcePriority","value":10,"confirmed":true,"guarded_interlock":true}'` |

#### ubLeadAcid_BatSubType - Subtype of lead-acid battery

| Choice | Application value | Mosquitto command |
|---|---:|---|
| USE | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":0,"confirmed":true}'` |
| SLD | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":1,"confirmed":true}'` |
| FLD | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":2,"confirmed":true}'` |
| GEL | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":3,"confirmed":true}'` |
| L14 | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":4,"confirmed":true}'` |
| L15 | `5` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":5,"confirmed":true}'` |
| L16 | `6` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":6,"confirmed":true}'` |
| N13 | `7` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":7,"confirmed":true}'` |
| N14 | `8` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":8,"confirmed":true}'` |
| LIT | `9` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubLeadAcid_BatSubType","value":9,"confirmed":true}'` |

#### Equalization - Equalization Mode Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Equalization","value":false,"confirmed":true}'` |
| Enable | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Equalization","value":true,"confirmed":true}'` |

#### EqualizationImmediately - Turn on Balanced Charging now

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EqualizationImmediately","value":false,"confirmed":true,"service_interlock":true}'` |
| Enable | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"EqualizationImmediately","value":true,"confirmed":true,"service_interlock":true}'` |

#### flcdEn - LCD setting enable bit

| Choice | Application value | Mosquitto command |
|---|---:|---|
| ECOMode_En (Energy Saving Mode Enabled) | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":0,"confirmed":true,"guarded_interlock":true}'` |
| OverLoad_RestartEn | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":1,"confirmed":true,"guarded_interlock":true}'` |
| OverTemp_RestartEn | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":2,"confirmed":true,"guarded_interlock":true}'` |
| InputChange_RemEn | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":3,"confirmed":true,"guarded_interlock":true}'` |
| OPSplit_PhaseEn | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":4,"confirmed":true,"guarded_interlock":true}'` |
| Generator_AutoIPEn | `5` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":5,"confirmed":true,"guarded_interlock":true}'` |
| DualChannel_LoadEn | `6` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":6,"confirmed":true,"guarded_interlock":true}'` |
| ubridFeedBackEn | `7` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"flcdEn","value":7,"confirmed":true,"guarded_interlock":true}'` |

#### DspBeepOnOff - Buzzer switch

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DspBeepOnOff","value":false,"confirmed":true}'` |
| Enable | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"DspBeepOnOff","value":true,"confirmed":true}'` |

#### OverloadToBypass - Overload transfer bypass enable

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OverloadToBypass","value":false,"confirmed":true,"guarded_interlock":true}'` |
| Enable | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"OverloadToBypass","value":true,"confirmed":true,"guarded_interlock":true}'` |

#### AcInputType - Off-grid output mode

| Choice | Application value | Mosquitto command |
|---|---:|---|
| APL | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcInputType","value":0,"confirmed":true}'` |
| UPS | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"AcInputType","value":1,"confirmed":true}'` |

#### Parallel_Mode - Parallel Mode

| Choice | Application value | Mosquitto command |
|---|---:|---|
| SIG | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":0,"confirmed":true}'` |
| PAL | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":1,"confirmed":true}'` |
| 3P1 | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":2,"confirmed":true}'` |
| 3P2 | `3` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":3,"confirmed":true}'` |
| 3P3 | `4` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Parallel_Mode","value":4,"confirmed":true}'` |

#### ubParallelDeviveType - Parallel Device Type

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Host | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubParallelDeviveType","value":0,"confirmed":true}'` |
| Slave | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubParallelDeviveType","value":1,"confirmed":true}'` |

#### ubBMSWorkMode - BMS communication method

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Disable | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBMSWorkMode","value":0,"confirmed":true}'` |
| can communication | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBMSWorkMode","value":1,"confirmed":true}'` |
| 485 communication | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBMSWorkMode","value":2,"confirmed":true}'` |

#### ubBluetoothEn - Bluetooth Enabled

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Bluetooth On | `true` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBluetoothEn","value":true,"confirmed":true}'` |
| Bluetooth Off | `false` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"ubBluetoothEn","value":false,"confirmed":true}'` |

#### upgrade notification - Data update notification, wifi re-submit 0304 data to the server

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Invalid default | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"upgrade notification","value":0,"confirmed":true,"service_interlock":true}'` |
| Trigger update | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"upgrade notification","value":1,"confirmed":true,"service_interlock":true}'` |

#### Datalogger Restart - wifi factory settings restored, server domain name and escalation time modified

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Invalid by default | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Datalogger Restart","value":0,"confirmed":true,"service_interlock":true}'` |
| Trigger a factory reset | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"Datalogger Restart","value":1,"confirmed":true,"service_interlock":true}'` |

#### LocalSafetyCmd - User Safety Selection Instructions

| Choice | Application value | Mosquitto command |
|---|---:|---|
| Regional Standard Safety Code | `0` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalSafetyCmd","value":0,"confirmed":true,"commissioning_interlock":true}'` |
| Subscriber Wide Range | `1` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalSafetyCmd","value":1,"confirmed":true,"commissioning_interlock":true}'` |
| Grid Company Safety Code | `2` | `mosquitto_pub -h "<broker>" -q 1 -t "<prefix>/<device-id>/inverter/<member-id>/command" -m '{"command_id":"<unique-command-id>","operation":"set","name":"LocalSafetyCmd","value":2,"confirmed":true,"commissioning_interlock":true}'` |

### NON-EXECUTABLE SETTERS - workbook-writable rows blocked by firmware (3)

These rows are included for protocol coverage but are not published as setter commands. Model validation and a future catalog-policy change are required before use.

| Name | Description | Domain | Access | Guard | Reason/notes |
|---|---|---|---|---|---|
| SystemSetBit | System Enable Flag Bit | system | read_write | blocked_unvalidated | Single and Triple Camera Use |
| ubConnectServer | Collector network status | communications | read_write | read_only | Low 8-bit indicates networking status:; 0x0055: Networking exception; 0x00AA: Networking normal; High 8-bit indicates collector type:; 0x0100: Wifi-U; 0x0200: 4G-U |
| ubDatalogAndArmCommunication | Collector and inverter communication status | communications | read_write | read_only | 0x55: Communication error; 0xAA: Communication OK |
<!-- END GENERATED COMPLETE COMMAND CATALOG -->
