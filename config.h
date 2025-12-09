#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// SERIAL CONFIGURATION
// ============================================
#define SERIAL_BAUD_RATE 115200

// ============================================
// WIFI CONFIGURATION (Multiple Networks)
// ============================================

struct WiFiCredentials {
  const char* ssid;
  const char* password;
};

const WiFiCredentials WIFI_NETWORKS[] = {
  {"ACT-ai_102757697732", "18788147"},
  {"Charlie", "logu9769"}
};
const int NUM_WIFI_NETWORKS = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// Keep for backward compatibility
#define WIFI_SSID WIFI_NETWORKS[0].ssid
#define WIFI_PASS WIFI_NETWORKS[0].password

// ============================================
// MQTT CONFIGURATION (Matching Dashboard)
// ============================================
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP32_SmartHome_"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

// ============================================
// MQTT TOPICS
// ============================================
#define MQTT_TOPIC_CONTROL "smarthome/control"
#define MQTT_TOPIC_LED_CONTROL "smarthome/led/control"
#define MQTT_TOPIC_STATUS "smarthome/status"

// ============================================
// PIN DEFINITIONS
// ============================================
#define RELAY1 10
#define RELAY2 11
#define RELAY3 12

#define PIN_LED_WS2812_DATA 38
#define PIN_BUTTON_ON_BOARD 4
#define PIN_BUZZER 16

// ============================================
// LED CONFIGURATION
// ============================================
#define NUM_LEDS 1
#define LED_BRIGHTNESS 50

// ============================================
// TIMING CONFIGURATION
// ============================================
#define STATUS_PUBLISH_INTERVAL 60000

// ============================================
// WEB SERVER HTML PAGE
// ============================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Smart Home</title>
...
</html>
)rawliteral";

#endif // CONFIG_H
