#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================
// GLOBAL OBJECTS
// ============================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_WS2812_DATA, NEO_GRB + NEO_KHZ800);

// ============================================
// STATE STRUCTURES
// ============================================
struct DeviceState {
  bool kitchenLight;
  bool bedroomLight;
  bool hallLight;
  bool buzzer;
} deviceState = {false, false, false, false};

struct LEDState {
  bool isOn;
  uint8_t brightness;
} ledState = {false, LED_BRIGHTNESS};

// ============================================
// GLOBAL VARIABLES
// ============================================
unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
int mqttReconnectAttempts = 0;

// ============================================
// FUNCTION DECLARATIONS
// ============================================
void setupWiFi();
void setupHardware();
void setupWebServer();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleLightControl(String message);
void handleLEDControl(String command);
void setLight(int room, bool state);
void setLED(bool state);
void setBuzzer(bool state);
void publishStatus();
void handleRoot();
void handleMQTT();
void handleToggle();
void handleAll();
void handleStatus();

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
  WiFi.begin(WIFI_SSID, WIFI_PASS);

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
// Add this function to your main.cpp after setupWiFi()

// ============================================
// TEST MQTT CONNECTION
// ============================================
void testMQTTConnection() {
  Serial.println("\n========================================");
  Serial.println("MQTT Connection Test");
  Serial.println("========================================");
  
  // Test 1: DNS Resolution
  Serial.print("Testing DNS resolution for: ");
  Serial.println(MQTT_BROKER);
  
  IPAddress brokerIP;
  if (WiFi.hostByName(MQTT_BROKER, brokerIP)) {
    Serial.print("✓ DNS OK - Broker IP: ");
    Serial.println(brokerIP);
  } else {
    Serial.println("✗ DNS FAILED - Can't resolve broker hostname");
    Serial.println("Possible fixes:");
    Serial.println("  1. Check your router's DNS settings");
    Serial.println("  2. Try broker IP directly: 91.121.93.94");
    Serial.println("  3. Try different broker: test.mosquitto.org");
    return;
  }
  
  // Test 2: TCP Connection
  Serial.print("Testing TCP connection to port ");
  Serial.print(MQTT_PORT);
  Serial.print("... ");
  
  WiFiClient testClient;
  if (testClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.println("✓ TCP OK");
    testClient.stop();
  } else {
    Serial.println("✗ TCP FAILED - Can't reach MQTT broker");
    Serial.println("Possible fixes:");
    Serial.println("  1. Check firewall settings");
    Serial.println("  2. Try port 8883 (secure) if available");
    Serial.println("  3. Your network might block MQTT");
    return;
  }
  
  // Test 3: MQTT Connection
  Serial.print("Testing MQTT handshake... ");
  
  String clientId = MQTT_CLIENT_ID;
  clientId += String(random(0xffff), HEX);
  
  bool connected = false;
  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    Serial.println("✓ MQTT OK - Connection successful!");
    mqttClient.disconnect();
  } else {
    Serial.print("✗ MQTT FAILED - Error code: ");
    Serial.println(mqttClient.state());
    Serial.println("\nError codes:");
    Serial.println("  -4 = Connection timeout");
    Serial.println("  -3 = Connection lost");
    Serial.println("  -2 = Connect failed");
    Serial.println("  -1 = Disconnected");
    Serial.println("   1 = Bad protocol");
    Serial.println("   2 = Bad client ID");
    Serial.println("   3 = Unavailable");
    Serial.println("   4 = Bad credentials");
    Serial.println("   5 = Unauthorized");
  }
  
  Serial.println("========================================\n");
}

// ============================================
// Add this to your setup() function AFTER setupWiFi()
// ============================================
// In setup(), add after setupWiFi():
// testMQTTConnection();  // Run diagnostic test

// ============================================
// HARDWARE SETUP
// ============================================
void setupHardware() {
  Serial.println("\n========================================");
  Serial.println("Hardware Initialization");
  Serial.println("========================================");
  
  // Relays (Active LOW)
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  digitalWrite(RELAY1, HIGH); // OFF
  digitalWrite(RELAY2, HIGH); // OFF
  digitalWrite(RELAY3, HIGH); // OFF
  Serial.println("✓ Relays: Kitchen(10), Bedroom(11), Hall(12)");
  
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
  
  // Welcome sequence
  Serial.println("\n🧪 Hardware test...");
  for(int i = 0; i < 3; i++) {
    strip.setPixelColor(0, strip.Color(255, 255, 255));
    strip.show();
    delay(100);
    strip.clear();
    strip.show();
    delay(100);
  }
  
  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_BUZZER, LOW);
  
  Serial.println("✓ Test complete");
  Serial.println("========================================\n");
}

// ============================================
// MQTT CALLBACK
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📨 MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == MQTT_TOPIC_CONTROL) {
    handleLightControl(message);
  }
  else if (String(topic) == MQTT_TOPIC_LED_CONTROL) {
    handleLEDControl(message);
  }
}

// ============================================
// LIGHT CONTROL HANDLER (MATCHING DASHBOARD)
// ============================================
void handleLightControl(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.print("🎛️  Processing: ");
  Serial.println(command);
  
  // Kitchen Light (matches dashboard: kitchen_light)
  if (command == "kitchen_light_on") {
    setLight(1, true);
  }
  else if (command == "kitchen_light_off") {
    setLight(1, false);
  }
  // Bedroom Light (matches dashboard: bed1_light)
  else if (command == "bed1_light_on") {
    setLight(2, true);
  }
  else if (command == "bed1_light_off") {
    setLight(2, false);
  }
  // Hall/Living Light (matches dashboard: living_light)
  else if (command == "living_light_on") {
    setLight(3, true);
  }
  else if (command == "living_light_off") {
    setLight(3, false);
  }
  // LED Control
  else if (command == "led_on") {
    setLED(true);
  }
  else if (command == "led_off") {
    setLED(false);
  }
  // All lights control
  else if (command == "all_on") {
    setLight(1, true);
    setLight(2, true);
    setLight(3, true);
  }
  else if (command == "all_off") {
    setLight(1, false);
    setLight(2, false);
    setLight(3, false);
  }
  // Buzzer
  else if (command == "buzzer_on") {
    setBuzzer(true);
  }
  // Status request
  else if (command == "status") {
    publishStatus();
  }
  else {
    Serial.println("⚠️  Unknown command");
  }
}

// ============================================
// LED CONTROL HANDLER (ON/OFF ONLY)
// ============================================
void handleLEDControl(String command) {
  Serial.print("💡 LED: ");
  Serial.println(command);
  
  command.toLowerCase();
  
  if (command == "on") {
    setLED(true);
  }
  else if (command == "off") {
    setLED(false);
  }
  else {
    Serial.println("⚠️  Unknown LED command (use 'on' or 'off')");
  }
}

// ============================================
// LIGHT CONTROL (Active LOW relays)
// ============================================
void setLight(int room, bool state) {
  int pin;
  bool* statePtr;
  const char* roomName;
  
  switch(room) {
    case 1: 
      pin = RELAY1; 
      statePtr = &deviceState.kitchenLight; 
      roomName = "Kitchen";
      break;
    case 2: 
      pin = RELAY2; 
      statePtr = &deviceState.bedroomLight; 
      roomName = "Bedroom";
      break;
    case 3: 
      pin = RELAY3; 
      statePtr = &deviceState.hallLight; 
      roomName = "Hall";
      break;
    default: 
      return;
  }
  
  *statePtr = state;
  digitalWrite(pin, state ? LOW : HIGH); // Active LOW
  
  Serial.printf("💡 %s %s\n", roomName, state ? "ON ✓" : "OFF");
  
  // Visual feedback via LED
  if (state) {
    strip.setBrightness(LED_BRIGHTNESS);
    strip.setPixelColor(0, strip.Color(255, 255, 255));
    strip.show();
    delay(150);
    if (!ledState.isOn) {
      strip.clear();
      strip.show();
    }
  }
  
  publishStatus();
}

// ============================================
// LED CONTROL (ON/OFF ONLY)
// ============================================
void setLED(bool state) {
  ledState.isOn = state;
  
  if (state) {
    strip.setBrightness(ledState.brightness);
    strip.setPixelColor(0, strip.Color(255, 255, 255)); // White only
    strip.show();
    Serial.println("✓ LED ON");
  } else {
    strip.clear();
    strip.show();
    Serial.println("✓ LED OFF");
  }
}

// ============================================
// BUZZER CONTROL
// ============================================
void setBuzzer(bool state) {
  if (state) {
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println("🔔 Buzzer ON");
    delay(500);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("🔔 Buzzer OFF");
  }
}

// ============================================
// PUBLISH STATUS (MATCHING DASHBOARD FORMAT)
// ============================================
void publishStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  StaticJsonDocument<512> doc;
  doc["kitchen_light"] = deviceState.kitchenLight ? "on" : "off";
  doc["bed1_light"] = deviceState.bedroomLight ? "on" : "off";
  doc["living_light"] = deviceState.hallLight ? "on" : "off";
  doc["led"] = ledState.isOn ? "on" : "off";
  doc["buzzer"] = deviceState.buzzer ? "on" : "off";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  if (mqttClient.publish(MQTT_TOPIC_STATUS, buffer, true)) {
    Serial.print("📤 Status: ");
    Serial.println(buffer);
  }
}

// ============================================
// MQTT RECONNECT
// ============================================
bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  Serial.print("🔌 MQTT... ");
  
  String clientId = MQTT_CLIENT_ID;
  clientId += String(random(0xffff), HEX);
  
  bool connected = false;
  if (strlen(MQTT_USERNAME) > 0) {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    Serial.println("✓ Connected!");
    mqttReconnectAttempts = 0;
    
    mqttClient.subscribe(MQTT_TOPIC_CONTROL);
    mqttClient.subscribe(MQTT_TOPIC_LED_CONTROL);
    Serial.println("✓ Subscribed");
    
    publishStatus();
    return true;
  } else {
    mqttReconnectAttempts++;
    Serial.print("❌ Failed (");
    Serial.print(mqttClient.state());
    Serial.println(")");
    return false;
  }
}

// ============================================
// WEB SERVER HANDLERS
// ============================================
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleMQTT() {
  if (server.hasArg("cmd")) {
    String command = server.arg("cmd");
    handleLightControl(command);
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Bad Request");
}

void handleToggle() {
  if (server.hasArg("relay")) {
    int relay = server.arg("relay").toInt();
    if (relay >= 1 && relay <= 3) {
      bool currentState = false;
      switch(relay) {
        case 1: currentState = deviceState.kitchenLight; break;
        case 2: currentState = deviceState.bedroomLight; break;
        case 3: currentState = deviceState.hallLight; break;
      }
      setLight(relay, !currentState);
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Bad Request");
}

void handleAll() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    bool newState = (state == "on");
    setLight(1, newState);
    setLight(2, newState);
    setLight(3, newState);
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Bad Request");
}

void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["kitchen"] = deviceState.kitchenLight;
  doc["bedroom"] = deviceState.bedroomLight;
  doc["hall"] = deviceState.hallLight;
  doc["led"] = ledState.isOn;
  doc["ip"] = WiFi.localIP().toString();
  doc["mqtt"] = mqttClient.connected() ? "Connected" : "Disconnected";
  doc["rssi"] = WiFi.RSSI();
  
  char buffer[512];
  serializeJson(doc, buffer);
  server.send(200, "application/json", buffer);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/mqtt", handleMQTT);
  server.on("/toggle", handleToggle);
  server.on("/all", handleAll);
  server.on("/status", handleStatus);
  server.begin();
  
  Serial.println("\n========================================");
  Serial.println("Web Server Started");
  Serial.println("========================================");
  Serial.print("Access: http://");
  Serial.println(WiFi.localIP());
  Serial.println("========================================\n");
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  SMART HOME CONTROLLER v3.0            ║");
  Serial.println("║  Kitchen + Bedroom + Hall Lights       ║");
  Serial.println("║  Web + MQTT Control                    ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  setupHardware();
  setupWiFi();
  setupWebServer();
  
  if (MDNS.begin("smarthome")) {
    Serial.println("✓ MDNS: http://smarthome.local");
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  
  Serial.println("\n========================================");
  Serial.println("📋 MQTT Commands:");
  Serial.println("========================================");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"kitchen_light_on\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"bed1_light_on\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"living_light_on\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"all_off\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"led_on\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"buzzer_on\"");
  Serial.println("========================================\n");
  
  randomSeed(micros());
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  server.handleClient();
  
  // WiFi check
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
    
    // Auto-publish status
    unsigned long now = millis();
    if (now - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
      lastStatusPublish = now;
      publishStatus();
    }
  }
  
  // Button control
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static int currentRoom = 1;
  bool currentButtonState = digitalRead(PIN_BUTTON_ON_BOARD);
  
  if (currentButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (currentButtonState == LOW && lastButtonState == HIGH) {
      bool currentState = false;
      switch(currentRoom) {
        case 1: currentState = deviceState.kitchenLight; break;
        case 2: currentState = deviceState.bedroomLight; break;
        case 3: currentState = deviceState.hallLight; break;
      }
      setLight(currentRoom, !currentState);
      
      currentRoom = (currentRoom % 3) + 1;
      
      digitalWrite(PIN_BUZZER, HIGH);
      delay(50);
      digitalWrite(PIN_BUZZER, LOW);
    }
  }
  
  lastButtonState = currentButtonState;
  delay(10);
}
