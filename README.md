# AutoCurtain Mark 2

ESP32-powered automatic curtain controller with Arduino IoT Cloud integration. Controls two independent motorized curtains via an L298N motor driver using quadrature encoder feedback — no physical limit switches required.

<p align="center">
  <img src="docs/images/enclosure-installed.jpg" alt="AutoCurtain Mark 2 installed on curtain rail" width="500">
</p>

## Features

- **Dual independent motors** — open and close each curtain separately
- **Encoder-based stall detection** — automatically detects open/closed limits without limit switches
- **Arduino IoT Cloud** — control and monitor from any device, anywhere
- **Scheduled routines** — automatic open at sunrise, close at sunset (configurable)
- **Physical button control** — instant local override with debounced buttons
- **PWM ramp control** — smooth motor acceleration to reduce curtain jerk
- **LED status indicators** — per-motor movement feedback + cloud connection status

## Hardware

<p align="center">
  <img src="docs/images/electronics-assembly.jpg" alt="Electronics assembly" width="650">
</p>

### Components

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32 DoiT DevKit V1 |
| Motor driver | L298N dual H-bridge |
| Motors | 2× DC motors with quadrature encoders |
| Enclosure | Custom 3D-printed |
| Inputs | 2× momentary push buttons |
| Indicators | 2× motor status LEDs, 1× cloud connection LED |

### Pin Map

| Signal | GPIO |
|--------|------|
| Motor 1 IN1 / IN2 / EN | 16, 17, 5 |
| Motor 2 IN1 / IN2 / EN | 18, 19, 4 |
| Encoder 1 A / B | 32, 33 |
| Encoder 2 A / B | 25, 26 |
| Button 1 / Button 2 | 21, 22 |
| LED 1 / LED 2 | 23, 27 |
| Connection LED | 2 |

## Getting Started

### Prerequisites

- [Arduino IDE 2.x](https://www.arduino.cc/en/software) or Arduino CLI
- ESP32 board support — install via Boards Manager: **esp32 by Espressif**
- Libraries (install via Library Manager):
  - `ArduinoIoTCloud`
  - `Arduino_ConnectionHandler`
- An [Arduino IoT Cloud](https://create.arduino.cc/iot) account with a configured Thing

### Setup

**1. Clone the repository**
```bash
git clone https://github.com/yourusername/curtainbotMARK2.git
cd curtainbotMARK2
```

**2. Create your secrets file**
```bash
cp firmware/curtainbot2_nov19a/arduino_secrets.h.template \
   firmware/curtainbot2_nov19a/arduino_secrets.h
```
Edit `arduino_secrets.h` and fill in your WiFi SSID, password, and the device secret key from your Arduino IoT Cloud Thing.

**3. Open the sketch**  
Open `firmware/curtainbot2_nov19a/curtainbot2_nov19a.ino` in Arduino IDE.

**4. Select board**  
Tools → Board → ESP32 Arduino → **DOIT ESP32 DEVKIT V1**

**5. Upload**  
Connect the ESP32 via USB and click Upload. Open the Serial Monitor at **115200 baud** to verify startup.

## Cloud Commands

Commands are sent by writing to the `stat_Messages` variable in the Arduino IoT Cloud dashboard:

| Command | Action |
|---------|--------|
| `H` | Print help menu |
| `I` | Show current schedule info |
| `MHH:MM` | Set morning open time (e.g. `M07:30`) |
| `Ms` / `Mr` | Pause / resume morning routine |
| `NHH:MM` | Set night close time (e.g. `N22:00`) |
| `Ns` / `Nr` | Pause / resume night routine |

## Motor State Machine

```
STOPPED ──open cmd──▶ MOVING_OPEN  ──stall detected──▶ OPEN
STOPPED ──close cmd─▶ MOVING_CLOSED──stall detected──▶ CLOSED
OPEN / CLOSED ──opposite cmd──▶ MOVING_* (ramps up from 0)
```

Stall detection monitors quadrature encoder counts at a configurable interval. If the count doesn't advance for `STALL_TIMEOUT` ms while the motor is commanded, the motor has reached its mechanical limit and locks into the `OPEN` or `CLOSED` state.

## Installation

<p align="center">
  <img src="docs/images/motor-detail.jpg" alt="Motor installed in curtain track" width="400">
</p>

The motor mounts inside the curtain valance and drives the existing curtain track directly. The 3D-printed enclosure houses the ESP32 and L298N and mounts at the end of the track.

## Project Structure

```
curtainbotMARK2/
├── firmware/
│   └── curtainbot2_nov19a/        # Active sketch — ESP32 + Arduino IoT Cloud
│       ├── curtainbot2_nov19a.ino
│       ├── control.h              # Motor class, encoder ISRs, button logic
│       ├── thingProperties.h      # Auto-generated cloud variable bindings
│       ├── arduino_secrets.h.template
│       └── webServer/             # Experimental web interface (not integrated)
├── legacy/
│   └── controlCode/               # Standalone sketch — no cloud, local control only
└── docs/
    └── images/
```

## Build via Arduino CLI

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32doit-devkit-v1 \
  firmware/curtainbot2_nov19a

arduino-cli upload \
  -p <PORT> \
  --fqbn esp32:esp32:esp32doit-devkit-v1 \
  firmware/curtainbot2_nov19a
```
