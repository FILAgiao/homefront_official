# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HomeFront is an ESP32-based smart garden irrigation controller. It manages solenoid valves and a water pump, supporting timed schedules, soil-moisture-based automation, and remote control via an IoT cloud platform.

## Build System

PlatformIO with the Arduino framework targeting the `esp32dev` board. VS Code is the recommended editor (extension: `platformio.platformio-ide`).

```bash
# Build the project
pio run

# Build and upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

There are no unit tests; the `test/` directory is a PlatformIO template placeholder.

## Dependencies

- **ArduinoJson 7.x** (`bblanchon/ArduinoJson@^7.4.2`) — JSON parsing for remote commands via TCP
- **EspSoftwareSerial 8.x** (`plerup/EspSoftwareSerial@^8.2.0`) — bit-bang UART for the RS485 soil moisture sensor

## Architecture

The entire application is a single file: `src/main.cpp` (~1345 lines). There are no custom headers or libraries. The project runs a main `loop()` with non-blocking WiFi reconnection and a Ticker-driven heartbeat at 40-second intervals.

### Hardware Pin Map

| Device              | GPIO Pin |
|---------------------|----------|
| Solenoid valve 1    | 27       |
| Solenoid valve 2    | 26       |
| Solenoid valve 3    | 25       |
| Water pump          | 18       |
| Soil sensor RX      | 17       |
| Soil sensor TX      | 16       |

Arrays `Solenoid_Pin[]`, `working_solenoid_valve[]`, and `pin_watering_time[]` are index-coupled — changing the order or length of one requires updating the others and the hard-coded index references in `loop()` for field/pool watering modes.

### Valve/Pump Control State Machine

`flag_execute()` runs a 5-state sequencer (`STATE_IDLE` → `STATE_OPEN_VALVE` → `STATE_PUMP_ON` → `STATE_CLOSE_PUMP` → `STATE_CLOSE_VALVE`) with `VALVE_DELAY` (5000ms) between each transition. This prevents water hammer by ensuring the pump never starts against a closed valve. During `STATE_PUMP_ON`, the state machine also handles valve switching — when transitioning to a different solenoid, it opens both old and new valves simultaneously for the delay period before closing the old one, keeping the pump running throughout.

### Watering Modes

1. **Auto-timing**: Triggers daily at `wat_begin_hour:wat_begin_min` (set via TCP), waters all valves in sequence
2. **Auto-soil**: Triggers when `soil_moisture < soil_moisture_need` (default 32%), waters all valves
3. **Manual (hand)**: Immediate one-shot watering of all valves
4. **Numeric command**: Single-valve debug mode via TCP (digit 1-N), with 10-minute timeout
5. **Car wash**: Special mode (GPIO-based physical trigger, code commented out)
6. **Field/pool**: Single-valve modes targeting specific solenoid index (6 or 5) — hard-coded, not parameterized

### TCP Protocol (tlink.io:8647)

The device connects to `tcp.tlink.io:8647` and identifies with a device ID (`7DV2YM9V6REVG96N`). Periodic heartbeats are sent every 40s (`"q"`). Incoming messages are parsed as:

| Format          | Action                                          |
|-----------------|-------------------------------------------------|
| `"A"`           | Heartbeat ack                                   |
| `"<N>%"`        | Set soil moisture threshold to N                |
| `"<N>x"`        | Set watering duration per valve (minutes)       |
| `"<H>v<M>"`     | Set daily watering start time (hour v minute)   |
| `"<1-N>"`       | Activate single solenoid (debug mode)           |
| JSON `{"key":v}`| Parsed via ArduinoJson for all other commands   |

JSON keys recognized: `carwash`, `auto_soil`, `auto_timing`, `hand`, `field`, `pool_2_wat`, `field_2_wat`, `corner_2_wat`, `shut`, `ota_upload`, `restart`.

Outbound telemetry is a `#`-delimited string: `solenoid_line, carwash_flag, auto_soil, auto_timing, hand, soil_moisture, pump_flag, time_status, reboot_flag, soil_threshold, pin_time[0], start_time, ota_status, 0, physical_buttons`.

### Network Resilience

WiFi reconnection is non-blocking. When the network is lost during watering (`soil2wat == 1`), the system continues watering using `millis()`-based time estimation. If WiFi stays lost without active watering, after ~500 failed reconnect attempts the ESP32 reboots.

### OTA Updates

Firmware updates via HTTP from `http://bin.bemfa.com/b/...` (Bemfa cloud). Triggered by the `ota_upload` JSON command.

## Key Constraints

- The `Solenoid_Pin` array has a fixed length of 3, but several code paths index it with values 4, 5, 6 (for field/corner/pool modes). These are out-of-bounds bugs unless the array is extended.
- The soil moisture sensor uses RS485 at 4800 baud via SoftwareSerial. `check_soil()` blocks for ~100ms per reading.
- `go_watering()` calls both `time2go()` and `soil_go()`, each of which calls `check_soil()` — so soil is polled twice in quick succession during watering decisions.
- `send2clinet()` references `pin_watering_time[6]` and `pin_watering_time[4]` which exceed the declared array size of 3.

## Hardware Reference

`docs/SCH_核心板_2026-03-12.pdf` contains the schematic for the core control board.
