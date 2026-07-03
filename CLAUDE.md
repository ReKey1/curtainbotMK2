# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CurtainBot MARK2 is an ESP32-based automatic curtain controller written in Arduino C++. It controls two motorized curtains via an L298N motor driver with quadrature encoder feedback. Control is fully local: the ESP32 hosts a minimal touch-friendly web UI + JSON API on the LAN, gets wall-clock time from NTP (Japan Standard Time, set in `config.h`), and runs two-stage morning/night routines. There is no cloud dependency (the Arduino IoT Cloud integration was removed).

## Build & Upload

This is an Arduino project — there are no traditional build scripts. Compile and upload via:
- **Arduino IDE**: Open `firmware/curtainbot2_nov19a/curtainbot2_nov19a.ino`, select board `ESP32 DoiT DevKit V1`, and upload.
- **Arduino CLI**: `arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 firmware/curtainbot2_nov19a && arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32doit-devkit-v1 firmware/curtainbot2_nov19a`
- **Serial monitor baud rate**: 115200
- Libraries: ESP32 core 3.x (`WebServer`, `Preferences`, `ledcAttach` API), ArduinoJson v7.

Before building: copy `firmware/curtainbot2_nov19a/arduino_secrets.h.template` to `arduino_secrets.h` and fill in `SECRET_SSID` / `SECRET_OPTIONAL_PASS`. Timezone is set to JST in `config.h` (`GMT_OFFSET_SEC`).

## Architecture

### Active Sketch: `firmware/curtainbot2_nov19a/`

| File | Role |
|------|------|
| `curtainbot2_nov19a.ino` | Thin orchestrator: `setup()` = controlSetup → calibrationBegin → WiFi → NTP → web server; `loop()` = controlLoop → handleClient → schedulerUpdate → calibrationUpdate. |
| `config.h` | Every tunable: HTTP port, WiFi timeout, NTP/timezone (JST), motor speed/ramp/direction flags, stall detection, go-to deadband, routine staging, schedule defaults. No magic numbers elsewhere. |
| `control.h` | `Motor` class + two globals `motor1` / `motor2`, encoder ISRs, `controlSetup()`, `controlLoop()` (buttons + LEDs). **Placement**: `motorRight = motor1` (GPIO 16/17/5), `motorLeft = motor2` (GPIO 18/19/4) — user-facing code uses the `motorLeft`/`motorRight` aliases. |
| `calibration.h` | Persists each motor's learned calibration (travel span, opening encoder sign, last endpoint state) to NVS (`Preferences`, namespace `curtainbot`). Auto-saves when a motor flags `consumeCalibDirty()`. |
| `scheduler.h` | NTP (`configTime`/`getLocalTime`) + `CurtainSchedule` morning/night with two-stage firing (pre-stage at T−`ROUTINE_LEAD_MIN`, main stage at T; each once per matching minute). |
| `commands.h` | Command registry: `COMMANDS[]` table of `{name, handler}`. Adding an action = one handler + one table row. `dispatchCommand()` returns OK / UNKNOWN / FAILED. |
| `webui.h` | PROGMEM barebones HTML page, `GET /` (page), `GET /status` (JSON), `POST /command` (JSON → dispatchCommand). |

Other folders: `docs/images/` (build photos used by the README), `MARK1/` (photos/videos of the previous MARK1 build — **not tracked by git**, do not delete).

### Motor State Machine

```
STOPPED ──open cmd──> MOVING_OPEN  ──stall at limit──> OPEN
STOPPED ──close cmd─> MOVING_CLOSED──stall at limit──> CLOSED
moveToPercent(pct) ──> MOVING_* ──target reached──> STOPPED
```

- **Self-tuning stall detection**: after the ramp, each move ratchets up its own cruise rate (highest encoder delta per `STALL_CHECK_MS` window — rises only, so a slipping belt can't drag it down); stall when the rate drops below `STALL_RATIO` of that cruise rate, never below the `STALL_MIN_TICKS` floor. Limits are inferred by stall, not switches.
- **Position tracking**: a full endpoint-to-endpoint travel measures `travelSpanTicks`; endpoint stalls re-anchor the tick position, so `positionPercent()` (0 = closed, 100 = open, -1 = uncalibrated) stays accurate. Calibration persists across reboots via `calibration.h`. If a stall happens more than `ENDPOINT_TOLERANCE_PCT` from the expected endpoint, it is treated as an obstruction (state STOPPED, calibration untouched).
- **Calibration bootstrap**: after first flash (or `recalibrate`), one full close then one full open teaches sign + span.

### Two-stage routines

Morning (open) / night (close) routines run quietly in two stages: `ROUTINE_LEAD_MIN` minutes (default 5) before the set time the curtains move to `ROUTINE_MID_PCT` (default 50%), then fully open/close exactly at the set time (which drives into the limit and re-anchors position tracking). The pre-stage only moves in the routine's direction and is skipped when uncalibrated; the main stage is skipped if already at the endpoint.

### Pin Map

| Signal | GPIO |
|--------|------|
| Motor1 (right curtain) IN1/IN2/EN | 16, 17, 5 |
| Motor2 (left curtain) IN1/IN2/EN | 18, 19, 4 |
| Encoder1 A/B | 32, 33 |
| Encoder2 A/B | 25, 26 |
| Button1 / Button2 | 21, 22 |
| LED1 / LED2 | 23, 27 |
| WiFi status LED | 2 |

### HTTP API

`POST /command` with JSON `{"cmd": "<name>", ...args}`; `GET /status` returns state, positions, schedules, `timeSynced`.

| Command | Args | Action |
|---------|------|--------|
| `open_left` / `close_left` / `stop_left` | — | Drive/stop left curtain (same for `_right`, `_both`) |
| `goto_left` / `goto_right` / `goto_both` | `pos` 0-100 | Move to absolute position (needs calibration) |
| `set_morning` / `set_night` | `hour`, `min`, `enabled` | Configure schedules |
| `recalibrate` | — | Forget span/position; next full close+open re-learns |

Physical buttons: press while moving stops; otherwise each press moves opposite to the last movement direction (always alternates).
