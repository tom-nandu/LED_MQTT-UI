#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"

// ============================================
// CONFIGURATION - UPDATE THESE VALUES
// ============================================
#define WIFI_SSID "ACT-ai_102757697732"
#define WIFI_PASSWORD "18788147"

#define MQTT_BROKER "broker.hivemq.com"  // or your broker IP
#define MQTT_PORT 1883
#define MQTT_USERNAME ""  // leave empty for public broker
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "ESP32_SmartHome_"

// MQTT Topics
#define MQTT_TOPIC_CONTROL "smarthome/control"
#define MQTT_TOPIC_STATUS "smarthome/status"

// LED Settings
#define NUM_LEDS 1
#define LED_BRIGHTNESS 255

// ============================================
// GLOBAL OBJECTS
// ============================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_WS2812_DATA, NEO_GRB + NEO_KHZ800);

// ============================================
// DEVICE STATES
// ============================================
struct DeviceState {
  bool livingLight;
  bool buzzer;
  uint8_t lightR;
  uint8_t lightG;
  uint8_t lightB;
} deviceState = {false, false, 255, 77, 77};  // Default red color

unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 30000;  // 30 seconds

// ============================================
// FUNCTION DECLARATIONS
// ============================================
void setupWiFi();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleDeviceControl(String message);
void setLivingLight(bool state);
void setBuzzer(bool state);
void publishStatus();
void setupHardware();

// ============================================
// WIFI SETUP
// ============================================
void setupWiFi() {
  delay(10);
  Serial.println("\n========================================");
  Serial.println("WiFi Connection");
  Serial.println("========================================");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi Failed! Restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("========================================\n");
}

// ============================================
// MQTT CALLBACK
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📨 Message [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == MQTT_TOPIC_CONTROL) {
    handleDeviceControl(message);
  }
}

// ============================================
// DEVICE CONTROL HANDLER
// ============================================
void handleDeviceControl(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.print("🎛️  Processing: ");
  Serial.println(command);
  
  // Living Hall Light Control
  if (command == "living_light_on" || command == "living_light:on") {
    setLivingLight(true);
  }
  else if (command == "living_light_off" || command == "living_light:off") {
    setLivingLight(false);
  }
  else if (command == "living_light_toggle" || command == "living_light:toggle") {
    setLivingLight(!deviceState.livingLight);
  }
  
  // Buzzer Control
  else if (command == "buzzer_on" || command == "buzzer:on") {
    setBuzzer(true);
  }
  else if (command == "buzzer_off" || command == "buzzer:off") {
    setBuzzer(false);
  }
  else if (command == "buzzer_toggle" || command == "buzzer:toggle") {
    setBuzzer(!deviceState.buzzer);
  }
  
  // Color presets for light
  else if (command == "light_red") {
    deviceState.lightR = 255; deviceState.lightG = 0; deviceState.lightB = 0;
    if (deviceState.livingLight) setLivingLight(true);
  }
  else if (command == "light_green") {
    deviceState.lightR = 0; deviceState.lightG = 255; deviceState.lightB = 0;
    if (deviceState.livingLight) setLivingLight(true);
  }
  else if (command == "light_blue") {
    deviceState.lightR = 0; deviceState.lightG = 0; deviceState.lightB = 255;
    if (deviceState.livingLight) setLivingLight(true);
  }
  else if (command == "light_white") {
    deviceState.lightR = 255; deviceState.lightG = 255; deviceState.lightB = 255;
    if (deviceState.livingLight) setLivingLight(true);
  }
  else if (command == "light_yellow") {
    deviceState.lightR = 255; deviceState.lightG = 255; deviceState.lightB = 0;
    if (deviceState.livingLight) setLivingLight(true);
  }
  
  else {
    Serial.println("⚠️  Unknown command");
  }
}

// ============================================
// LIVING HALL LIGHT CONTROL (WS2812B)
// ============================================
void setLivingLight(bool state) {
  if (deviceState.livingLight == state && state) {
    Serial.println("ℹ️  Light already in requested state");
    return;
  }
  
  deviceState.livingLight = state;
  
  if (state) {
    strip.setBrightness(LED_BRIGHTNESS);
    strip.setPixelColor(0, strip.Color(deviceState.lightR, deviceState.lightG, deviceState.lightB));
    strip.show();
    Serial.printf("💡 Living Light ON - RGB(%d,%d,%d)\n", 
                  deviceState.lightR, deviceState.lightG, deviceState.lightB);
  } else {
    strip.clear();
    strip.show();
    Serial.println("💡 Living Light OFF");
  }
  
  publishStatus();
}

// ============================================
// BUZZER CONTROL
// ============================================
void setBuzzer(bool state) {
  deviceState.buzzer = state;
  
  if (state) {
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println("🔔 Buzzer ON");
    
    // Auto turn off after 2 seconds to prevent continuous buzzing
    delay(2000);
    digitalWrite(PIN_BUZZER, LOW);
    deviceState.buzzer = false;
    Serial.println("🔔 Buzzer OFF (auto)");
  } else {
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("🔔 Buzzer OFF");
  }
  
  publishStatus();
}

// ============================================
// PUBLISH STATUS TO MQTT
// ============================================
void publishStatus() {
  if (!mqttClient.connected()) {
    Serial.println("⚠️  Cannot publish - MQTT disconnected");
    return;
  }
  
  char msg[200];
  snprintf(msg, sizeof(msg),
    "{\"living_light\":\"%s\",\"buzzer\":\"%s\",\"light_rgb\":[%d,%d,%d]}",
    deviceState.livingLight ? "on" : "off",
    deviceState.buzzer ? "on" : "off",
    deviceState.lightR,
    deviceState.lightG,
    deviceState.lightB
  );
  
  if (mqttClient.publish(MQTT_TOPIC_STATUS, msg, true)) {
    Serial.print("📤 Status published: ");
    Serial.println(msg);
  } else {
    Serial.println("❌ Publish failed");
  }
}

// ============================================
// MQTT RECONNECT
// ============================================
bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi not connected");
    return false;
  }
  
  Serial.print("🔌 MQTT connecting... ");
  
  String clientId = MQTT_CLIENT_ID;
  clientId += String(random(0xffff), HEX);
  
  bool connected = false;
  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    Serial.println("✓ MQTT Connected!");
    
    if (mqttClient.subscribe(MQTT_TOPIC_CONTROL)) {
      Serial.print("  ✓ Subscribed: ");
      Serial.println(MQTT_TOPIC_CONTROL);
    }
    
    publishStatus();
    return true;
  } else {
    Serial.print("❌ Failed, rc=");
    Serial.println(mqttClient.state());
    return false;
  }
}

// ============================================
// HARDWARE SETUP
// ============================================
void setupHardware() {
  Serial.println("\n========================================");
  Serial.println("Hardware Initialization");
  Serial.println("========================================");
  
  // Button
  pinMode(PIN_BUTTON_ON_BOARD, INPUT_PULLUP);
  Serial.println("✓ Button (GPIO 4)");
  
  // Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("✓ Buzzer (GPIO 16)");
  
  // WS2812B LED
  strip.begin();
  strip.show();
  strip.setBrightness(LED_BRIGHTNESS);
  Serial.println("✓ WS2812B LED (GPIO 38)");
  
  // Test sequence
  Serial.println("\n🧪 Running hardware test...");
  
  // Test LED
  strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.show();
  delay(300);
  strip.setPixelColor(0, strip.Color(0, 255, 0));
  strip.show();
  delay(300);
  strip.setPixelColor(0, strip.Color(0, 0, 255));
  strip.show();
  delay(300);
  strip.clear();
  strip.show();
  
  // Test buzzer
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  
  Serial.println("✓ Hardware test complete");
  Serial.println("========================================\n");
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   ESP32 Smart Home Controller v1.0     ║");
  Serial.println("║   Living Hall Light + Buzzer Control   ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  setupHardware();
  setupWiFi();
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  
  Serial.println("\n========================================");
  Serial.println("📋 MQTT Test Commands:");
  Serial.println("========================================");
  Serial.println("Living Light:");
  Serial.println("  mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"living_light_on\"");
  Serial.println("  mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"living_light_off\"");
  Serial.println("\nBuzzer:");
  Serial.println("  mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"buzzer_on\"");
  Serial.println("========================================\n");
  
  randomSeed(micros());
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // WiFi reconnection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi lost! Reconnecting...");
    setupWiFi();
  }
  
  // MQTT connection
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnectMQTT();
    }
  } else {
    mqttClient.loop();
    
    // Periodic status publish
    unsigned long now = millis();
    if (now - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
      lastStatusPublish = now;
      publishStatus();
    }
  }
  
  // Button control (local toggle)
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  bool currentButtonState = digitalRead(PIN_BUTTON_ON_BOARD);
  
  if (currentButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (currentButtonState == LOW && lastButtonState == HIGH) {
      Serial.println("🔘 Button pressed - toggling light");
      setLivingLight(!deviceState.livingLight);
      
      // Brief buzzer feedback
      digitalWrite(PIN_BUZZER, HIGH);
      delay(100);
      digitalWrite(PIN_BUZZER, LOW);
    }
  }
  
  lastButtonState = currentButtonState;
  
  delay(10);
}
