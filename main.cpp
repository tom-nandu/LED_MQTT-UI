#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>

// ============================================
// PIN DEFINITIONS
// ============================================
#define RELAY1 10  // Kitchen Light
#define RELAY2 11  // Bedroom Light
#define RELAY3 12  // Hall Light
#define PIN_BUTTON_ON_BOARD 4
#define PIN_BUZZER 16
#define PIN_LED_WS2812_DATA 38
#define NUM_LEDS 1

// ============================================
// CONFIGURATION
// ============================================
const char* WIFI_SSID = "ACT-ai_102757697732";
const char* WIFI_PASSWORD = "18788147";

const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_CLIENT_ID = "ESP32_HomeControl_";

// MQTT Topics
const char* MQTT_TOPIC_CONTROL = "smarthome/control";
const char* MQTT_TOPIC_STATUS = "smarthome/status";
const char* MQTT_TOPIC_LED_CONTROL = "smarthome/led/control";

const char* DEVICE_NAME = "TOM_SMART_HOME";
const int SERIAL_BAUD_RATE = 115200;
const uint8_t LED_BRIGHTNESS = 50;

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
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  bool changed;
} ledState = {false, 255, 255, 255, LED_BRIGHTNESS, false};

// ============================================
// GLOBAL VARIABLES
// ============================================
unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
const unsigned long STATUS_PUBLISH_INTERVAL = 30000;
int mqttReconnectAttempts = 0;

// ============================================
// HTML WEB INTERFACE
// ============================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Home Control</title>
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; background: #1a1a2e; color: #eee; }
    h1 { color: #16c8bb; }
    .container { max-width: 500px; margin: auto; }
    .light-card { background: #16213e; padding: 20px; margin: 15px 0; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .button { width: 100%; padding: 15px; font-size: 18px; border: none; border-radius: 8px; cursor: pointer; transition: 0.3s; }
    .btn-on { background: #4CAF50; color: white; }
    .btn-off { background: #f44336; color: white; }
    .btn-on:hover { background: #45a049; }
    .btn-off:hover { background: #da190b; }
    .status { margin-top: 10px; font-weight: bold; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🏠 Smart Home Control</h1>
    
    <div class="light-card">
      <h2>🍳 Kitchen Light</h2>
      <button class="button btn-on" onclick="toggleLight(1)">Toggle</button>
      <div class="status" id="status1">OFF</div>
    </div>
    
    <div class="light-card">
      <h2>🛏️ Bedroom Light</h2>
      <button class="button btn-on" onclick="toggleLight(2)">Toggle</button>
      <div class="status" id="status2">OFF</div>
    </div>
    
    <div class="light-card">
      <h2>🚪 Hall Light</h2>
      <button class="button btn-on" onclick="toggleLight(3)">Toggle</button>
      <div class="status" id="status3">OFF</div>
    </div>
    
    <div class="light-card">
      <button class="button" style="background:#ff9800" onclick="allLights('on')">All ON</button>
      <button class="button" style="background:#607d8b;margin-top:10px" onclick="allLights('off')">All OFF</button>
    </div>
  </div>
  
  <script>
    function toggleLight(num) {
      fetch('/toggle?relay=' + num).then(() => updateStatus());
    }
    function allLights(state) {
      fetch('/all?state=' + state).then(() => updateStatus());
    }
    function updateStatus() {
      fetch('/status').then(r => r.json()).then(data => {
        document.getElementById('status1').innerText = data.kitchen ? 'ON' : 'OFF';
        document.getElementById('status1').style.color = data.kitchen ? '#4CAF50' : '#f44336';
        document.getElementById('status2').innerText = data.bedroom ? 'ON' : 'OFF';
        document.getElementById('status2').style.color = data.bedroom ? '#4CAF50' : '#f44336';
        document.getElementById('status3').innerText = data.hall ? 'ON' : 'OFF';
        document.getElementById('status3').style.color = data.hall ? '#4CAF50' : '#f44336';
      });
    }
    setInterval(updateStatus, 2000);
    updateStatus();
  </script>
</body>
</html>
)rawliteral";

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
void setLED(bool state, uint8_t r, uint8_t g, uint8_t b);
void setBuzzer(bool state);
void publishStatus();
void publishLEDStatus();
void handleRoot();
void handleToggle();
void handleAll();
void handleStatus();
void printMQTTError(int errorCode);

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
  
  // Welcome beep
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  
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
    handleLightControl(message);
  }
  else if (String(topic) == MQTT_TOPIC_LED_CONTROL) {
    handleLEDControl(message);
  }
}

// ============================================
// LIGHT CONTROL HANDLER
// ============================================
void handleLightControl(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.print("🎛️  Processing: ");
  Serial.println(command);
  
  // Kitchen Light (Relay 1)
  if (command == "kitchen_light_on" || command == "kitchen_light:on") {
    setLight(1, true);
  }
  else if (command == "kitchen_light_off" || command == "kitchen_light:off") {
    setLight(1, false);
  }
  else if (command == "kitchen_light_toggle" || command == "kitchen_toggle") {
    setLight(1, !deviceState.kitchen_light);
  }
  
  // Bedroom Light (Relay 2)
  else if (command == "bed1_light_on" || command == "bed1_light:on") {
    setLight(2, true);
  }
  else if (command == "bed1_light_off" || command == "bed1_light:off") {
    setLight(2, false);
  }
  else if (command == "bed1_light_toggle" || command == "bed1_toggle") {
    setLight(2, !deviceState.bed1_light);
  }
  
  // Living Hall Light (Relay 3)
  else if (command == "living_light_on" || command == "living_light:on") {
    setLight(3, true);
  }
  else if (command == "living_light_off" || command == "living_light:off") {
    setLight(3, false);
  }
  else if (command == "living_light_toggle" || command == "living_toggle") {
    setLight(3, !deviceState.living_light);
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
  else if (command == "buzzer_on" || command == "buzzer:on") {
    setBuzzer(true);
  }
  else if (command == "buzzer_off" || command == "buzzer:off") {
    setBuzzer(false);
  }
  
  // Status
  else if (command == "status") {
    publishStatus();
  }
  
  else {
    Serial.println("⚠️  Unknown command");
  }
}

// ============================================
// LED CONTROL HANDLER
// ============================================
void handleLEDControl(String command) {
  Serial.print("💡 LED Command: ");
  Serial.println(command);
  
  if (command == "on") {
    setLED(true, ledState.red, ledState.green, ledState.blue);
  }
  else if (command == "off") {
    setLED(false, 0, 0, 0);
  }
  else if (command == "red") {
    setLED(true, 255, 0, 0);
  }
  else if (command == "green") {
    setLED(true, 0, 255, 0);
  }
  else if (command == "blue") {
    setLED(true, 0, 0, 255);
  }
  else if (command == "white") {
    setLED(true, 255, 255, 255);
  }
  else if (command == "yellow") {
    setLED(true, 255, 255, 0);
  }
  else if (command == "cyan") {
    setLED(true, 0, 255, 255);
  }
  else if (command == "magenta") {
    setLED(true, 255, 0, 255);
  }
}

// ============================================
// LIGHT CONTROL (Active LOW)
// ============================================
void setLight(int room, bool state) {
  int pin;
  bool* statePtr;
  const char* roomName;
  
  switch(room) {
    case 1: 
      pin = RELAY1; 
      statePtr = &deviceState.kitchen_light; 
      roomName = "Kitchen";
      break;
    case 2: 
      pin = RELAY2; 
      statePtr = &deviceState.bed1_light; 
      roomName = "Bedroom 1";
      break;
    case 3: 
      pin = RELAY3; 
      statePtr = &deviceState.living_light; 
      roomName = "Living Hall";
      break;
    default: 
      return;
  }
  
  *statePtr = state;
  digitalWrite(pin, state ? LOW : HIGH); // Active LOW
  
  Serial.printf("💡 %s Light %s\n", roomName, state ? "ON" : "OFF");
  
  // Visual feedback
  if (state) {
    strip.setBrightness(LED_BRIGHTNESS);
    strip.setPixelColor(0, strip.Color(0, 255, 0));
    strip.show();
    delay(100);
    strip.clear();
    strip.show();
  }
  
  publishStatus();
}

// ============================================
// LED CONTROL
// ============================================
void setLED(bool state, uint8_t r, uint8_t g, uint8_t b) {
  ledState.isOn = state;
  ledState.changed = true;
  
  if (state) {
    ledState.red = r;
    ledState.green = g;
    ledState.blue = b;
    
    strip.setBrightness(ledState.brightness);
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
    
    Serial.printf("✓ LED ON - RGB(%d, %d, %d)\n", r, g, b);
  } else {
    strip.clear();
    strip.show();
    Serial.println("✓ LED OFF");
  }
  
  publishLEDStatus();
}

// ============================================
// BUZZER CONTROL
// ============================================
void setBuzzer(bool state) {
  deviceState.buzzer = state;
  
  if (state) {
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println("🔔 Buzzer ON");
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
// PUBLISH STATUS
// ============================================
void publishStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  char msg[350];
  snprintf(msg, sizeof(msg),
    "{\"kitchen_light\":\"%s\",\"bed1_light\":\"%s\",\"living_light\":\"%s\",\"buzzer\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}",
    deviceState.kitchen_light ? "on" : "off",
    deviceState.bed1_light ? "on" : "off",
    deviceState.living_light ? "on" : "off",
    deviceState.buzzer ? "on" : "off",
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI()
  );
  
  if (mqttClient.publish(MQTT_TOPIC_STATUS, msg, true)) {
    Serial.print("📤 Status: ");
    Serial.println(msg);
  }
}

void publishLEDStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  char msg[200];
  snprintf(msg, sizeof(msg),
    "{\"led_state\":\"%s\",\"r\":%d,\"g\":%d,\"b\":%d}",
    ledState.isOn ? "on" : "off",
    ledState.red,
    ledState.green,
    ledState.blue
  );
  
  mqttClient.publish("home/led/status", msg, true);
}

// ============================================
// MQTT RECONNECT
// ============================================
void printMQTTError(int errorCode) {
  Serial.print("Error: ");
  switch(errorCode) {
    case -4: Serial.println("TIMEOUT"); break;
    case -3: Serial.println("CONNECTION LOST"); break;
    case -2: Serial.println("CONNECT FAILED"); break;
    case -1: Serial.println("DISCONNECTED"); break;
    case 1: Serial.println("BAD PROTOCOL"); break;
    case 2: Serial.println("BAD CLIENT ID"); break;
    case 3: Serial.println("UNAVAILABLE"); break;
    case 4: Serial.println("BAD CREDENTIALS"); break;
    case 5: Serial.println("UNAUTHORIZED"); break;
    default: Serial.println("UNKNOWN"); break;
  }
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
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
    Serial.println("✓ Connected!");
    mqttReconnectAttempts = 0;
    
    mqttClient.subscribe(MQTT_TOPIC_CONTROL);
    mqttClient.subscribe(MQTT_TOPIC_LED_CONTROL);
    Serial.println("✓ Subscribed to topics");
    
    publishStatus();
    return true;
  } else {
    mqttReconnectAttempts++;
    Serial.print("❌ Failed - ");
    printMQTTError(mqttClient.state());
    return false;
  }
}

// ============================================
// WEB SERVER HANDLERS
// ============================================
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
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
  char json[250];
  snprintf(json, sizeof(json),
    "{\"kitchen_light\":%s,\"bed1_light\":%s,\"living_light\":%s,\"ip\":\"%s\"}",
    deviceState.kitchen_light ? "true" : "false",
    deviceState.bed1_light ? "true" : "false",
    deviceState.living_light ? "true" : "false",
    WiFi.localIP().toString().c_str()
  );
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/all", handleAll);
  server.on("/status", handleStatus);
  server.begin();
  
  Serial.println("\n========================================");
  Serial.println("Web Server Started");
  Serial.println("========================================");
  Serial.print("Access at: http://");
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
  Serial.println("║   TOM NANDU SMART HOME v3.0            ║");
  Serial.println("║   3-Room Light Control + MQTT          ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  setupHardware();
  setupWiFi();
  setupWebServer();
  
  // MDNS
  if (MDNS.begin("tomfcb")) {
    Serial.println("✓ MDNS: http://tomfcb.local");
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(60);
  
  Serial.println("\n========================================");
  Serial.println("📋 MQTT Commands:");
  Serial.println("========================================");
  Serial.println("Kitchen:    mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"kitchen_light_on\"");
  Serial.println("Bedroom 1:  mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"bed1_light_on\"");
  Serial.println("Living:     mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"living_light_on\"");
  Serial.println("All ON:     mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"all_on\"");
  Serial.println("Buzzer:     mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"buzzer_on\"");
  Serial.println("LED Red:    mosquitto_pub -h broker.hivemq.com -t smarthome/led/control -m \"red\"");
  Serial.println("========================================\n");
  
  randomSeed(micros());
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Web server
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
    
    // Periodic status
    unsigned long now = millis();
    if (now - lastStatusPublish > STATUS_PUBLISH_INTERVAL) {
      lastStatusPublish = now;
      publishStatus();
    }
  }
  
  // Button control (cycles through rooms)
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
        case 1: currentState = deviceState.kitchen_light; break;
        case 2: currentState = deviceState.bed1_light; break;
        case 3: currentState = deviceState.living_light; break;
      }
      setLight(currentRoom, !currentState);
      
      currentRoom = (currentRoom % 3) + 1;
      
      // Button feedback
      digitalWrite(PIN_BUZZER, HIGH);
      delay(50);
      digitalWrite(PIN_BUZZER, LOW);
    }
  }
  
  lastButtonState = currentButtonState;
  
  delay(10);
}
