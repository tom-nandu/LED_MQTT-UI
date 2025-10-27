#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "ACT-ai_102757697732"
#define WIFI_PASSWORD "18788147"

// MQTT Broker Configuration
#define MQTT_BROKER "broker.hivemq.com"  // Change to your broker IP
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "iiot-v4"
#define MQTT_USERNAME ""  // Leave empty if no auth
#define MQTT_PASSWORD ""  // Leave empty if no auth

// MQTT Topics


#define MQTT_TOPIC_LED_STATUS "home/led/status"
#define MQTT_TOPIC_LED_CONTROL "homeled/control"
#define MQTT_TOPIC_COMMAND "home/led/command"

// Device Configuration
#define DEVICE_NAME "IIOT_V4_Board"
#define SERIAL_BAUD_RATE 115200
#define PUBLISH_INTERVAL 5000  // milliseconds

// WS2812B LED Configuration
#define NUM_LEDS 1  // Number of LEDs in your strip
#define LED_BRIGHTNESS 50  // 0-255

// Feature Flags
#define ENABLE_BUZZER true
#define ENABLE_WS2812B true
#define ENABLE_SD_CARD false
#define ENABLE_RS485 false

#endif // CONFIG_H