# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

HomeFront is an ESP32-based smart garden irrigation controller. It drives solenoid valves and water pumps via 74HC595 shift registers, with a 0.96" SSD1306 OLED (Chinese UI), EC11 rotary encoder, and 3 physical buttons for local control. It supports timed schedules, soil-moisture-based automation, and remote control via TCP long connection to tlink.io.

## Build System

PlatformIO with Arduino framework targeting `esp32dev`. Run commands from `HomeFront_esp32/`.

```bash
# Build
pio run

# Build and upload
pio run --target upload

# Serial monitor
pio device monitor
```

No unit tests exist; `test/` is a PlatformIO template placeholder.

## Dependencies

- **ArduinoJson 7.x** (`bblanchon/ArduinoJson@^7.4.2`) — JSON parsing for TCP remote commands
- **U8g2 2.x** (`olikraus/U8g2@^2.35.30`) — OLED display with Chinese font (wqy12_t_gb2312)

## Architecture

```
HomeFront_esp32/src/
├── main.cpp                    (~310 lines) Global variable definitions, setup(), loop()
├── pinmap.h                    All GPIO pin definitions
├── hal/
│   ├── relay.h / relay.cpp     74HC595 16-bit shift register driver (2 cascaded)
│   ├── oled.h / oled.cpp       SSD1306 OLED, Chinese wqy12 font, 5-row scrolling
│   ├── encoder.h / encoder.cpp EC11 rotary encoder (GPIO interrupts)
│   └── buttons.h / buttons.cpp 3-key input (debounce + long-press detection)
├── core/
│   ├── globals.h               Central extern declarations (~46 globals), MAX_VALVES=6, MAX_PUMPS=2
│   ├── config.h / config.cpp   NVS load/save + AP mode web config portal
│   ├── sensor.h / sensor.cpp   RS485 soil moisture sensor (Modbus RTU, hardware Serial, 4800 baud)
│   ├── network.h / network.cpp Non-blocking WiFi + TCP reconnection
│   ├── protocol.h / protocol.cpp TCP message parsing + telemetry sender
│   ├── watering.h / watering.cpp 5-state valve/pump sequencer + watering decision logic
│   └── ota.h / ota.cpp         HTTP firmware update
└── ui/
    └── menu.h / menu.cpp       7-page Chinese menu system with numeric editor
```

`main.cpp` defines all global variables (externs declared in `core/globals.h`). Setup initializes relay, menu (OLED+encoder+buttons), loads config from NVS, connects WiFi, and opens TCP to tlink.io. The loop runs `menu_tick()` for UI, then WiFi/TCP reconnection, time sync, heartbeat, incoming message handling, watering decisions, and the flag state machine.

### Hardware Pin Map

See `pinmap.h` for the authoritative pin definitions. Key assignments:

| Device              | GPIO  | Notes                              |
|---------------------|-------|------------------------------------|
| 595 DS (data)       | 13    | Shift register serial data         |
| 595 STCP (latch)    | 12    | Shift register latch (boot pin)    |
| 595 SHCP (clock)    | 14    | Shift register clock               |
| OLED SDA            | 21    | I2C data                           |
| OLED SCL            | 22    | I2C clock                          |
| RS485 TX            | 25    | Serial2 UART to RS485 transceiver  |
| RS485 RX            | 26    | Serial2 UART from RS485 transceiver|
| EC11 A              | 19    | Encoder phase A                    |
| EC11 B              | 18    | Encoder phase B                    |
| EC11 KEY            | 32    | Encoder button                     |
| KEY1                | 4     | Return/Back                        |
| KEY2                | 27    | Quick control shortcut             |
| KEY3                | 33    | Emergency stop                     |

### 74HC595 Relay Control

Two cascaded 74HC595s provide 16 output channels via 3 GPIOs (DS/STCP/SHCP). Channels 0-5 are small relays (RLY1-RLY6, 120mA) for solenoid valves. Channels 6-7 are large relays (RLY7-RLY8, 185mA) for pumps. The `relay_set(channel, on/off)` function handles bit-level updates with latch pulsing. `relay_init()` clears all channels on startup.

### Valve/Pump Control State Machine

`flag_execute()` (in `core/watering.cpp`) runs a 5-state sequencer with `VALVE_DELAY` (5000ms) between transitions:

```
STATE_IDLE → STATE_OPEN_VALVE → STATE_PUMP_ON → STATE_CLOSE_PUMP → STATE_CLOSE_VALVE
```

This staggered timing prevents water hammer — the pump never starts against a closed valve. During `STATE_PUMP_ON`, when switching between valves (e.g., completing valve 1 and starting valve 2), the new valve opens while the old one remains open for the delay period, then the old one closes. This keeps the pump running throughout the transition.

### Watering Modes

1. **Auto-timing**: Triggers daily at `wat_begin_hour:wat_begin_min`, waters all valves sequentially
2. **Auto-soil**: Triggers when `soil_moisture < soil_moisture_need` (default 32%), waters all valves
3. **Manual (hand)**: Immediate one-shot watering of all valves
4. **Car wash**: Runs pump for `carwash_duration_min` minutes (default 30), no specific valve
5. **Field (vegetable)**: Single-valve mode targeting `field_valve_num` (1-based, configurable), for `pin_watering_time[field_valve_num-1]` minutes
6. **Numeric command**: Single-valve debug mode via TCP (digit 1-N), 10-minute timeout

### Runtime-Configurable Hardware

`valve_count` (1-6) and `pump_count` (0-2) are stored in NVS and configurable via the on-device menu or web portal. This allows standardized PCB production with per-installation configuration. All loops that iterate over valves/pumps use these counts rather than compile-time constants. `MAX_VALVES=6` and `MAX_PUMPS=2` are the hard PCB limits defined in `core/globals.h`.

### NVS Configuration

All 15 parameters are persisted in the `"homefront"` NVS namespace via `Preferences`:

- WiFi: `ssid`, `password`, `device_id`, `upUrl`
- Hardware: `valve_cnt` (1-6), `pump_cnt` (0-2), `field_valve` (1-based)
- Watering: `wat_t0`..`wat_t5` (per-valve minutes, 0-120), `wat_hour` (0-23), `wat_min` (0-59)
- Sensors: `soil_need` (float, 0-100)
- Automation: `auto_time` (0/1), `auto_soil` (0/1)
- Special: `carwash_dur` (1-120 minutes)

`saveAllConfig()` is called on mode toggles, parameter edits, and system save. On first boot with empty `ssid`, the device enters AP mode (`startConfigPortal()`) serving an HTML form at `http://192.168.4.1/`.

### OLED Menu System

7 pages navigated via EC11 encoder (rotate=move, press=select/enter) and 3 buttons (KEY1=back, KEY2=quick control, KEY3=emergency stop):

| Page          | Content                                           |
|---------------|---------------------------------------------------|
| Main          | 状态信息, 阀门控制, 模式选择, 参数设置, 系统管理    |
| Status        | Soil moisture, threshold, mode, time (read-only)  |
| Control       | Per-valve and per-pump on/off toggles (dynamic)   |
| Mode          | Auto-timing toggle, auto-soil toggle, manual/carwash/field triggers |
| Params        | All 15 numeric parameters with encoder editing    |
| System        | Save, WiFi reconnect, about, restart, factory reset |
| About         | Version, WiFi status, IP, device ID               |

The params page uses a `ParamInfo` struct (`label`, `int* value`, `min_val`, `max_val`) with a numeric editor mode: press to edit, rotate to adjust, press to commit (with `constrain()` and `saveAllConfig()`). `soil_moisture_need` uses a static int proxy for float conversion since `ParamInfo` stores `int*`.

### TCP Protocol (tlink.io:8647)

Device connects to `tcp.tlink.io:8647`, identifies with `device_id`, and sends heartbeat `"q"` every 40s via Ticker.

Incoming messages:
| Format          | Action                                    |
|-----------------|-------------------------------------------|
| `"A"`           | Heartbeat ack                             |
| `"<N>%"`        | Set soil moisture threshold to N          |
| `"<N>x"`        | Set watering duration per valve (minutes) |
| `"<H>v<M>"`     | Set daily watering start time             |
| `"<1-N>"`       | Activate single solenoid (debug, 10min timeout) |
| JSON `{"key":v}`| Parsed via ArduinoJson for named commands |

JSON keys: `carwash`, `auto_soil`, `auto_timing`, `hand`, `field`, `shut`, `ota_upload`, `restart`.

Outbound telemetry is a `#`-delimited string with 15 fields: solenoid_line, carwash_flag, auto_soil, auto_timing, hand, soil_moisture, pump_flag, time_status, reboot_flag, soil_threshold, pin_time[0], start_time, ota_status, 0, physical_buttons.

### Network Resilience

WiFi and TCP reconnection are non-blocking (`wifi_reconnect_cx()`, `check_client_connected()`), called each loop iteration. When network is lost during active watering (`soil2wat == 1`), watering continues via `millis()`-based time estimation. If WiFi stays lost without active watering, after ~500 failed reconnect attempts the ESP32 reboots.

### OTA Updates

Firmware updates via HTTP GET from `upUrl` (configurable). Triggered by the `ota_upload` JSON command. Uses Arduino `Update` class with progress/error callbacks logged to Serial2.

## Key Constraints

- The soil moisture sensor uses RS485 at 4800 baud via hardware Serial (UART0, GPIO1/3). `check_soil()` blocks for ~100ms per reading. Serial is shared with the ESP32 download port — disconnect RS485 when flashing.
- `go_watering()` calls both `time2go()` and `soil_go()`, each of which calls `check_soil()`, so soil is polled twice in quick succession during watering decisions.
- `ParamInfo` stores `int*` pointers — for `soil_moisture_need` (a `float`), the menu uses a static int proxy variable. Ensure this proxy stays in sync with the actual float value.
- When `valve_count` changes in the params editor, `field_valve_num` is automatically constrained to the new range.
- The encoder ISR runs on GPIO18/19 (dual-phase Gray code state machine, CHANGE interrupts). Internal pull-ups are enabled.
- Buttons use `INPUT_PULLUP` — press = LOW. Debounce is 50ms, long-press threshold is 800ms.
- `oled_draw_page()` uses a compile-time `VISIBLE_ROWS=5` and scrolls the visible window centered on cursor. No dynamic allocation — all line buffers must be valid for the duration of the call.

## Hardware Reference

`docs/SCH_核心板_2026-03-12.pdf` contains the schematic for the core control board.
