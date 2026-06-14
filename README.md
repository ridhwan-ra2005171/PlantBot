# PlantBot 🌱

An autonomous plant monitoring system built with ESP8266 for the Interactive Systems course.

---

## What it does

Monitors soil moisture, temperature, and humidity in real time. Sends alerts to Telegram when the plant needs water. Uses an ultrasonic sensor to wake the display when someone approaches.

---

## Hardware

| Component | Purpose |
|-----------|---------|
| ESP8266 NodeMCU | Main controller |
| OLED display 128x64 | Shows sensor readings |
| Capacitive moisture sensor | Soil moisture measurement |
| DHT11 | Temperature & humidity |
| HC-SR04 | Proximity detection (wakes display) |
| Red/Yellow/Green LEDs | Visual status indicator |

---

## Wiring

```
D1 (GPIO5)  → OLED SCL
D2 (GPIO4)  → OLED SDA
D4 (GPIO2)  → DHT11 data
D5 (GPIO14) → Green LED
D6 (GPIO12) → Ultrasonic TRIG
D7 (GPIO13) → Red LED
D8 (GPIO15) → Yellow LED
D0 (GPIO16) → Ultrasonic ECHO
A0          → Moisture sensor (via voltage divider)
```

> **Note:** Moisture sensor needs a voltage divider (two 10k resistors) because ESP8266 A0 only handles 0-1V.

---

## Features

- Real-time moisture, temperature, and humidity monitoring
- OLED display with auto wake-up via proximity sensor
- LED status indicators (red = dry, yellow = low, green = good)
- Telegram bot for remote monitoring and alerts
- Moisture history (last 10 readings)
- Custom alert thresholds and plant naming

---

## Telegram Commands

| Command | Description |
|---------|-------------|
| `/status` | Current sensor readings |
| `/water` | Mark plant as watered |
| `/history` | Last 10 moisture readings |
| `/environment` | Temperature & humidity details |
| `/setname <name>` | Set custom plant name |
| `/setalert <value>` | Set moisture alert threshold |
| `/displayon` | Keep display on for 1 hour |
| `/help` | Show all commands |

---

## Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP8266 board support
3. Install libraries via Library Manager:
   - `Adafruit SSD1306`
   - `Adafruit GFX`
   - `DHT sensor library`
   - `UniversalTelegramBot`
   - `ArduinoJson`
4. Copy `credentials_example.h` → rename to `credentials.h`
5. Fill in your WiFi and Telegram credentials in `credentials.h`
6. Upload `plant_monitor_final.ino` to ESP8266

---

## Credentials

Credentials are stored in `credentials.h` which is excluded from this repo via `.gitignore`.

Use `credentials_example.h` as a template:

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* botToken = "YOUR_TELEGRAM_BOT_TOKEN";
const char* chatID = "YOUR_TELEGRAM_CHAT_ID";
```

---

## Calibration

**Moisture sensor** (values found by testing):
- Dry (in air): `95`
- Wet (in water): `64`

**DHT11 sensor** (cheap sensor, needed offset correction):
- Temperature offset: `+20.0°C`
- Humidity offset: `+35.0%`

---

## How it works

1. Reads all sensors every 500ms
2. Updates OLED with current values
3. Controls LEDs based on moisture:
   - 🔴 Red → below 20% (very dry)
   - 🟡 Yellow → 20-40% (dry)
   - 🟢 Green → above 40% (good)
4. Wakes display when someone is within 50cm
5. Checks Telegram for commands every second
6. Sends alert when moisture drops below threshold

---

## Files

| File | Description |
|------|-------------|
| `plant_monitor_final.ino` | Main code (use this one) |
| `complete_plant_robot.ino` | Full version with all sensors |
| `moisture_telegram_bot.ino` | Telegram + moisture only |
| `sketch_1.ino` | First working version |
| `calibrate_moisture.ino` | Tool to find calibration values |
| `led_test.ino` | Test LEDs independently |
| `test_dht.ino` | Test DHT sensor |
| `dht_auto_detect.ino` | Try both DHT11 and DHT22 |
| `credentials_example.h` | Template for credentials |

---

## Known Issues

- DHT11 sensor requires manual calibration (likely a cheap unit)
- History limited to 10 readings

---

## Future Plans

- Better temperature sensor (DS18B20)
- Light sensor for sunlight tracking
- Web dashboard

---


