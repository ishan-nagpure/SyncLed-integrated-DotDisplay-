#pragma once

// ─── WiFi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID      "YOUR_SSID"
#define WIFI_PASSWORD  "YOUR_PASSWORD"

// ─── MQTT (set 0 to disable) ──────────────────────────────────────────────────
#define MQTT_ENABLED   0
#define MQTT_SERVER    "192.168.1.100"
#define MQTT_PORT      1883
#define MQTT_USER      ""
#define MQTT_PASSWORD  ""
#define MQTT_CLIENT_ID "esp32-led"

// ─── WS2812B LED Strip ────────────────────────────────────────────────────────
#define LED_PIN        16
#define LED_COUNT      30
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define MAX_MILLIAMPS  2000

// ─── MAX7219 Dot Matrix ───────────────────────────────────────────────────────
#define MATRIX_DIN     23
#define MATRIX_CLK     18
#define MATRIX_CS       5
#define MATRIX_DEVICES  4
#define MATRIX_HW_TYPE MD_MAX72XX::FC16_HW   // try PAROLA_HW if text looks garbled
#define MATRIX_BRIGHT   5       // 0–15
#define MATRIX_SPEED   40       // scroll speed ms per step (lower = faster)

// ─── DHT22 ────────────────────────────────────────────────────────────────────
#define DHT_PIN        14
#define DHT_TYPE       DHT22
#define DHT_INTERVAL   10000UL
#define USE_CELSIUS    true

// ─── NTP Time ─────────────────────────────────────────────────────────────────
#define NTP_SERVER     "pool.ntp.org"
#define TZ_OFFSET_SEC  19800    // IST = UTC+5:30 = 19800 seconds
#define DST_OFFSET_SEC 0

// ─── Matrix display cycle (ms each slot stays before moving to next) ───────────
#define CYCLE_TIME_MS  12000UL

// ─── Web server ───────────────────────────────────────────────────────────────
#define WEB_PORT       80
