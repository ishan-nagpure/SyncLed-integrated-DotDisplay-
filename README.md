# MatrixCore32

> All-in-one ESP32 firmware — WS2812B LED effects, MAX7219 dot matrix display, DHT22 sensor, NTP clock, stopwatch, countdown timer, and a web control panel. No app needed.

---

## Features

- **20 LED effects** on a WS2812B strip — Fire, Rainbow, Larson Scanner, Meteor Rain, Confetti and more
- **MAX7219 4-in-1 dot matrix** with 7 display modes:
  - Stable clock (HH:MM with blinking colon, IST timezone)
  - Scrolling time + temperature + humidity
  - Motivational quotes (15 built-in, auto-cycling)
  - Pixel shapes — Heart, Smiley, Arrow, Star, Music Note, Sun
  - Live stopwatch with tenths of seconds
  - Live countdown timer
  - Custom text via web UI
- **DHT22** temperature and humidity sensor
- **NTP time sync** — no RTC module needed (IST / UTC+5:30 by default)
- **Web control panel** — works from any browser on the same WiFi, no app or installation needed
- **Stopwatch** — start, pause, lap, reset with lap history
- **Countdown timer** — set hours/minutes/seconds, pause/resume, flashes LED strip red on completion
- **MQTT support** — optional, for Home Assistant integration
- **Boot screen** — scrolls and displays IP address clearly so you always know where to connect
- **LED alert flashes** — green on lap, red when timer finishes

---

## Hardware required

| Component | Details |
|---|---|
| ESP32 DevKit | Any WROOM-32 based board |
| WS2812B LED strip | Any length, set count in config.h |
| MAX7219 4-in-1 dot matrix | Blue PCB FC16 type (most common) |
| DHT22 sensor | Also sold as AM2302 |
| 10kΩ resistor | Pull-up for DHT22 data line |
| 5V power supply | Rated for your LED strip (0.3W per LED at full brightness) |
| Jumper wires | — |

---

## Wiring

### MAX7219 → ESP32

| MAX7219 Pin | ESP32 GPIO |
|---|---|
| VCC | 5V (external supply) |
| GND | GND |
| DIN | GPIO 23 |
| CLK | GPIO 18 |
| CS | GPIO 5 |

### DHT22 → ESP32

| DHT22 Pin | ESP32 |
|---|---|
| VCC | 3.3V |
| DATA | GPIO 14 |
| GND | GND |

> Place a **10kΩ resistor** between DHT22 DATA and VCC. The sensor will not work without it.

### WS2812B → ESP32

| Strip | ESP32 |
|---|---|
| 5V | External 5V supply |
| GND | GND (shared with ESP32 and supply) |
| DIN | GPIO 16 |

> **Important:** Power the LED strip and MAX7219 from an external 5V supply, not from the ESP32's 5V pin. At full brightness a 30-LED strip draws around 1.8A — far beyond what USB can provide.

---

## Software setup

### Prerequisites

- [VS Code](https://code.visualstudio.com)
- [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)

### Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/YOUR_USERNAME/MatrixCore32.git
   cd MatrixCore32
   ```

2. Open the folder in VS Code:
   ```
   File → Open Folder → select MatrixCore32
   ```

3. Edit `src/config.h` — set your WiFi credentials and LED count:
   ```cpp
   #define WIFI_SSID     "your_wifi_name"
   #define WIFI_PASSWORD "your_wifi_password"
   #define LED_COUNT     30    // change to your actual LED count
   #define LED_PIN       16    // GPIO connected to strip data line
   ```

4. Click the **→ Upload** button in the bottom blue bar of VS Code. PlatformIO will download all libraries automatically and flash the firmware.

---

## First boot

After flashing, the matrix display will show:

```
Booting...
↓
Connect to:  192.168.x.x    ← scrolls slowly
↓
192.168.x.x                 ← static for 5 seconds
```

Open that IP address in any browser on the same WiFi network. The web control panel loads instantly — no app, no Bluetooth pairing needed.

---

## Web control panel

The built-in web UI gives you full control from any device:

| Section | Controls |
|---|---|
| Sensor | Live temperature and humidity readout |
| Clock | Current IST time |
| Stopwatch | Start / Pause / Lap / Reset, lap history |
| Timer | Set H:M:S, Start / Pause / Reset |
| LED Strip | Power toggle, brightness, colour picker, 20 effect buttons |
| Matrix Display | 7 mode buttons, auto-cycle toggle, custom message, scroll speed, brightness |

---

## Configuration reference (`src/config.h`)

```cpp
// WiFi
WIFI_SSID           — your network name
WIFI_PASSWORD       — your network password

// LED Strip
LED_PIN             — GPIO for strip data (default: 16)
LED_COUNT           — number of LEDs in your strip (default: 30)
MAX_MILLIAMPS       — safety current cap in mA (default: 2000)

// MAX7219
MATRIX_DIN/CLK/CS   — SPI pins (default: 23, 18, 5)
MATRIX_DEVICES      — number of 8x8 modules (default: 4)
MATRIX_HW_TYPE      — FC16_HW for blue PCB, PAROLA_HW for red PCB
MATRIX_BRIGHT       — brightness 0–15 (default: 5)
MATRIX_SPEED        — scroll speed ms/step (default: 40, lower = faster)

// DHT22
DHT_PIN             — GPIO for sensor data (default: 14)
DHT_INTERVAL        — ms between reads (default: 10000)
USE_CELSIUS         — true for °C, false for °F

// NTP
NTP_SERVER          — time server (default: pool.ntp.org)
TZ_OFFSET_SEC       — timezone offset in seconds (default: 19800 = IST)

// MQTT (optional)
MQTT_ENABLED        — set to 1 to enable
MQTT_SERVER         — broker IP
MQTT_PORT           — default 1883

// Matrix
CYCLE_TIME_MS       — ms each auto-cycle mode stays on (default: 12000)
```

---

## MQTT topics (optional)

Enable MQTT by setting `MQTT_ENABLED 1` in `config.h`.

| Topic | Direction | Payload | Description |
|---|---|---|---|
| `led/effect` | Subscribe | `0`–`19` | Set LED effect by number |
| `led/power` | Subscribe | `on` / `off` | Turn strip on or off |
| `matrix/text` | Subscribe | any string | Show custom text on matrix |
| `matrix/mode` | Subscribe | `0`–`6` | Set matrix display mode |
| `sensor/temperature` | Publish | float | Temperature reading |
| `sensor/humidity` | Publish | float | Humidity reading |

---

## LED effects list

| # | Name | # | Name |
|---|---|---|---|
| 0 | Solid | 10 | Sparkle |
| 1 | Breathing | 11 | Fire |
| 2 | Color Cycle | 12 | Comet |
| 3 | Rainbow | 13 | Strobe |
| 4 | Rainbow Cycle | 14 | Sunrise |
| 5 | Color Wipe | 15 | Meteor Rain |
| 6 | Theater Chase | 16 | Confetti |
| 7 | Running Lights | 17 | Police |
| 8 | Larson Scanner | 18 | Candle |
| 9 | Twinkle | 19 | Wipe Random |

---

## Matrix display modes

| # | Mode | Description |
|---|---|---|
| 0 | Clock + Temp | Scrolls current time, temperature and humidity |
| 1 | Quote | Cycles through 15 motivational quotes |
| 2 | Shape | Shows pixel shapes — Heart, Smiley, Arrow, Star, Note, Sun |
| 3 | Custom | Scrolls your own text set from the web UI |
| 4 | Stopwatch | Live stopwatch display with tenths of seconds |
| 5 | Timer | Live countdown timer display |
| 6 | Clock | Stable static HH:MM with blinking colon |

---

## Troubleshooting

**Matrix shows garbled pixels**
Change `MATRIX_HW_TYPE` in `config.h` from `MD_MAX72XX::FC16_HW` to `MD_MAX72XX::PAROLA_HW` — there are two common PCB variants.

**DHT22 always shows sensor error**
Check that the 10kΩ pull-up resistor is connected between DATA and VCC. Without it the sensor will not respond.

**WiFi not connecting**
- SSID and password are case sensitive
- ESP32 only supports 2.4GHz — it cannot connect to 5GHz networks
- Check serial monitor output at 115200 baud for error messages

**LED strip not working**
- Make sure GND of the external power supply is connected to ESP32 GND
- Check `LED_PIN` and `LED_COUNT` match your hardware
- Try swapping `COLOR_ORDER` from `GRB` to `RGB` in `config.h` if colours look wrong

**Web page not loading**
- Make sure your phone/PC is on the same WiFi network as the ESP32
- Check the IP address on the matrix display or in the serial monitor
- Try `http://` not `https://`

---

## Project structure

```
MatrixCore32/
├── platformio.ini        — build config and library dependencies
├── README.md             — this file
├── .gitignore            — excludes build artifacts
└── src/
    ├── config.h          — all user settings (WiFi, pins, LED count)
    ├── effects.h         — 20 WS2812B LED effect implementations
    ├── matrix_content.h  — pixel shapes, quotes, display mode enum
    └── main.cpp          — main firmware (WiFi, web server, matrix, sensor, timer)
```

---

## Libraries used

| Library | Author | Version |
|---|---|---|
| FastLED | FastLED | ^3.6.0 |
| MD_Parola | MajicDesigns | ^3.7.3 |
| MD_MAX72XX | MajicDesigns | ^3.5.1 |
| DHT sensor library | Adafruit | ^1.4.6 |
| Adafruit Unified Sensor | Adafruit | ^1.1.14 |
| ArduinoJson | Benoit Blanchon | ^7.0.4 |
| PubSubClient | Nick O'Leary | ^2.8.0 |

All libraries are downloaded automatically by PlatformIO on first build.

---

## License

MIT License — free to use, modify and distribute.

---

## Contributing

Pull requests welcome. If you add a new LED effect or matrix mode, please update the effect/mode tables in this README.
