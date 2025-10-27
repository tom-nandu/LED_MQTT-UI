#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>  // MUST come BEFORE webserver.h
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include "pins.h"
#include "config.h"
#include "ledserver.h"   // This comes AFTER WebServer.h
#include "user_roles.h"


// Global objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED_WS2812_DATA, NEO_GRB + NEO_KHZ800);

// Global variables
unsigned long lastPublish = 0;
unsigned long lastReconnectAttempt = 0;
int messageCount = 0;
int mqttReconnectAttempts = 0;

// LED State structure
struct LEDState {
  bool isOn;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  bool changed;
} ledState = {false, 0, 0, 0, LED_BRIGHTNESS, false};

// ========================================
// Function Declarations
// ========================================
void setupWiFi();
void testNetworkConnectivity();
void testMQTTBrokerReachability();
bool reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleCommand(String command);
void handleLEDControl(String command);
void setLED(bool state, uint8_t r, uint8_t g, uint8_t b);
void publishLEDStatus();
void publishStatus();
void publishData();
void setupHardware();
void printMQTTError(int errorCode);

// ========================================
// WiFi Functions
// ========================================
void setupWiFi() {
  delay(10);
  Serial.println("\n========================================");
  Serial.println("WiFi Connection Details:");
  Serial.println("========================================");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi Connected Successfully!");
    Serial.println("----------------------------------------");
    Serial.print("IP Address:    ");
    Serial.println(WiFi.localIP());
    Serial.print("Subnet Mask:   ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway:       ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS Server:    ");
    Serial.println(WiFi.dnsIP());
    Serial.print("MAC Address:   ");
    Serial.println(WiFi.macAddress());
    Serial.print("RSSI:          ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Channel:       ");
    Serial.println(WiFi.channel());
    Serial.println("========================================");
  } else {
    Serial.println("✗ WiFi Connection Failed!");
    Serial.print("WiFi Status Code: ");
    Serial.println(WiFi.status());
    Serial.println("Rebooting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }
}

// ========================================
// Network Diagnostics
// ========================================
void testNetworkConnectivity() {
  Serial.println("\n========================================");
  Serial.println("Network Connectivity Tests:");
  Serial.println("========================================");
  
  // Test 1: Ping Gateway
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  
  // Test 2: DNS Resolution
  IPAddress testIP;
  Serial.print("Testing DNS resolution... ");
  if (WiFi.hostByName("google.com", testIP)) {
    Serial.print("✓ SUCCESS - google.com resolves to ");
    Serial.println(testIP);
  } else {
    Serial.println("✗ FAILED - DNS not working");
  }
  
  // Test 3: Check if on same subnet as broker
  IPAddress brokerIP;
  if (WiFi.hostByName(MQTT_BROKER, brokerIP)) {
    Serial.print("MQTT Broker IP: ");
    Serial.println(brokerIP);
    
    IPAddress localIP = WiFi.localIP();
    IPAddress subnet = WiFi.subnetMask();
    
    // Check if same subnet
    bool sameSubnet = true;
    for (int i = 0; i < 4; i++) {
      if ((localIP[i] & subnet[i]) != (brokerIP[i] & subnet[i])) {
        sameSubnet = false;
        break;
      }
    }
    
    Serial.print("Same subnet as broker: ");
    Serial.println(sameSubnet ? "YES" : "NO");
  } else {
    Serial.print("Attempting direct IP connection to: ");
    Serial.println(MQTT_BROKER);
  }
  
  Serial.println("========================================\n");
}

void testMQTTBrokerReachability() {
  Serial.println("\n========================================");
  Serial.println("MQTT Broker Reachability Test:");
  Serial.println("========================================");
  Serial.print("Broker Address: ");
  Serial.println(MQTT_BROKER);
  Serial.print("Broker Port:    ");
  Serial.println(MQTT_PORT);
  
  WiFiClient testClient;
  testClient.setTimeout(5000); // 5 second timeout
  
  Serial.print("Attempting TCP connection... ");
  
  unsigned long startTime = millis();
  bool connected = testClient.connect(MQTT_BROKER, MQTT_PORT);
  unsigned long duration = millis() - startTime;
  
  if (connected) {
    Serial.println("✓ SUCCESS!");
    Serial.print("Connection established in ");
    Serial.print(duration);
    Serial.println(" ms");
    Serial.println("TCP socket is working correctly.");
    testClient.stop();
    Serial.println("✓ Broker is reachable and accepting connections");
  } else {
    Serial.println("✗ FAILED!");
    Serial.print("Connection attempt took ");
    Serial.print(duration);
    Serial.println(" ms");
    Serial.println("\nPossible issues:");
    Serial.println("  1. MQTT broker not running on target host");
    Serial.println("  2. Firewall blocking port 1883");
    Serial.println("  3. Wrong IP address or hostname");
    Serial.println("  4. Network routing issue");
    Serial.println("  5. Broker only accepting localhost connections");
    Serial.println("\nTroubleshooting steps:");
    Serial.println("  - Verify broker is running: mosquitto -v");
    Serial.println("  - Check broker config allows external connections");
    Serial.println("  - Test from command line: telnet " + String(MQTT_BROKER) + " " + String(MQTT_PORT));
    Serial.println("  - Try public broker: broker.hivemq.com");
  }
  Serial.println("========================================\n");
}

// ========================================
// MQTT Error Code Decoder
// ========================================
void printMQTTError(int errorCode) {
  Serial.print("MQTT Error Code: ");
  Serial.print(errorCode);
  Serial.print(" - ");
  
  switch(errorCode) {
    case -4:
      Serial.println("MQTT_CONNECTION_TIMEOUT - Server didn't respond within keepalive time");
      break;
    case -3:
      Serial.println("MQTT_CONNECTION_LOST - Network cable unplugged");
      break;
    case -2:
      Serial.println("MQTT_CONNECT_FAILED - Network connection failed");
      Serial.println("  → Cannot reach broker (check IP, port, firewall)");
      break;
    case -1:
      Serial.println("MQTT_DISCONNECTED - Cleanly disconnected");
      break;
    case 0:
      Serial.println("MQTT_CONNECTED - Connection successful");
      break;
    case 1:
      Serial.println("MQTT_CONNECT_BAD_PROTOCOL - Protocol version not supported");
      break;
    case 2:
      Serial.println("MQTT_CONNECT_BAD_CLIENT_ID - Client ID rejected");
      break;
    case 3:
      Serial.println("MQTT_CONNECT_UNAVAILABLE - Server unavailable");
      break;
    case 4:
      Serial.println("MQTT_CONNECT_BAD_CREDENTIALS - Bad username/password");
      break;
    case 5:
      Serial.println("MQTT_CONNECT_UNAUTHORIZED - Not authorized to connect");
      break;
    default:
      Serial.println("UNKNOWN ERROR CODE");
      break;
  }
}

// ========================================
// MQTT Callback
// ========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == MQTT_TOPIC_COMMAND) {
    handleCommand(message);
  }
  else if (String(topic) == MQTT_TOPIC_LED_CONTROL) {
    handleLEDControl(message);
  }
}

// ========================================
// Command Handlers
// ========================================
void handleCommand(String command) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

  Serial.printf("📥 Command received at %s: %s\n", timestamp, command.c_str());
  Serial.print("Processing command: ");
  Serial.println(command);
  
  if (command == "buzzer_on" && ENABLE_BUZZER) {
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println("✓ Buzzer ON");
  } 
  else if (command == "buzzer_off" && ENABLE_BUZZER) {
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("✓ Buzzer OFF");
  }
  else if (command == "status") {
    publishStatus();
  }
  else if (command == "led_status") {
    publishLEDStatus();
  }
  else if (command == "restart") {
    Serial.println("⚠ Restart command received. Rebooting...");
    delay(1000);
    ESP.restart();
  }
  else if (command == "test_network") {
    testNetworkConnectivity();
    testMQTTBrokerReachability();
  }
}
  
void handleLEDControl(String command) {
  Serial.print("LED Command: ");
  Serial.println(command);
  
  if (!ENABLE_WS2812B) {
    Serial.println("✗ WS2812B not enabled");
    return;
  }
  
  if (command == "on") {
    if (ledState.red == 0 && ledState.green == 0 && ledState.blue == 0) {
      setLED(true, 255, 255, 255);
    } else {
      setLED(true, ledState.red, ledState.green, ledState.blue);
    }
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
  else if (command.startsWith("{")) {
    int rPos = command.indexOf("\"r\":");
    int gPos = command.indexOf("\"g\":");
    int bPos = command.indexOf("\"b\":");
    
    if (rPos > 0 && gPos > 0 && bPos > 0) {
      int r = command.substring(rPos + 4).toInt();
      int g = command.substring(gPos + 4).toInt();
      int b = command.substring(bPos + 4).toInt();
      setLED(true, r, g, b);
    }
  }
}

// ========================================
// LED Control Functions
// ========================================
void setLED(bool state, uint8_t r, uint8_t g, uint8_t b) {
  bool hasChanged = (ledState.isOn != state) || 
                    (ledState.red != r) || 
                    (ledState.green != g) || 
                    (ledState.blue != b);
  
  if (!hasChanged) {
    Serial.println("ℹ No LED change detected");
    return;
  }
  
  ledState.isOn = state;
  ledState.changed = true;
  
  if (state) {
    ledState.red = r;
    ledState.green = g;
    ledState.blue = b;
    
    strip.setBrightness(ledState.brightness);
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
    
    Serial.printf("✓ LED ON - RGB(%d, %d, %d) - #%02X%02X%02X\n", r, g, b, r, g, b);
  } else {
    strip.clear();
    strip.show();
    Serial.println("✓ LED OFF");
  }
}

void publishLEDStatus() {
  if (!mqttClient.connected()) {
    Serial.println("✗ Cannot publish LED status - MQTT not connected");
    return;
  }
  
  char msg[200];
  
  const char* colorName = "custom";

  if (ledState.red == 255 && ledState.green == 0 && ledState.blue == 0) {
    colorName = "red";
  } else if (ledState.red == 0 && ledState.green == 255 && ledState.blue == 0) {
    colorName = "green";
  } else if (ledState.red == 0 && ledState.green == 0 && ledState.blue == 255) {
    colorName = "blue";
  } else if (ledState.red == 255 && ledState.green == 255 && ledState.blue == 255) {
    colorName = "white";
  } else if (ledState.red == 255 && ledState.green == 255 && ledState.blue == 0) {
    colorName = "yellow";
  } else if (ledState.red == 0 && ledState.green == 255 && ledState.blue == 255) {
    colorName = "cyan";
  } else if (ledState.red == 255 && ledState.green == 0 && ledState.blue == 255) {
    colorName = "magenta";
  }

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

  snprintf(msg, sizeof(msg),
    "{\"state\":\"%s\",\"color\":\"%s\",\"timestamp\":\"%s\"}",
    ledState.isOn ? "on" : "off",
    colorName,
    timestamp
  );
  
  if (mqttClient.publish(MQTT_TOPIC_LED_STATUS, msg, true)) {
    Serial.print("✓ LED Status Published: ");
    Serial.println(msg);
    ledState.changed = false;
  } else {
    Serial.println("✗ Failed to publish LED status");
  }
}

// ========================================
// MQTT Functions with Better Error Handling
// ========================================
bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi not connected. Cannot connect to MQTT.");
    return false;
  }
  
  // Limit reconnection attempts
  if (mqttReconnectAttempts > 0 && mqttReconnectAttempts % 10 == 0) {
    Serial.println("\n⚠ Multiple MQTT connection failures detected.");
    Serial.println("Running network diagnostics...");
    testNetworkConnectivity();
    testMQTTBrokerReachability();
  }
  
  Serial.println("\n----------------------------------------");
  Serial.print("MQTT Connection Attempt #");
  Serial.println(mqttReconnectAttempts + 1);
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  String clientId = MQTT_CLIENT_ID;
  clientId += String(random(0xffff), HEX);
  Serial.print("Client ID: ");
  Serial.println(clientId);
  
  bool connected = false;
  
  Serial.print("Connecting... ");
  if (strlen(MQTT_USERNAME) > 0) {
    Serial.println("(with authentication)");
    connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  } else {
    Serial.println("(no authentication)");
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    Serial.println("✓✓✓ MQTT CONNECTED SUCCESSFULLY! ✓✓✓");
    mqttReconnectAttempts = 0;
    
    // Subscribe to topics
    Serial.println("Subscribing to topics...");
    if (mqttClient.subscribe(MQTT_TOPIC_COMMAND)) {
      Serial.print("  ✓ ");
      Serial.println(MQTT_TOPIC_COMMAND);
    }
    
    if (mqttClient.subscribe(MQTT_TOPIC_LED_CONTROL)) {
      Serial.print("  ✓ ");
      Serial.println(MQTT_TOPIC_LED_CONTROL);
    }
    
    // Publish initial status
    publishStatus();
    publishLEDStatus();
    
    Serial.println("----------------------------------------\n");
    return true;
    
  } else {
    mqttReconnectAttempts++;
    Serial.println("✗ MQTT CONNECTION FAILED!");
    printMQTTError(mqttClient.state());
    Serial.println("----------------------------------------\n");
    return false;
  }
}

void publishStatus() {
  if (!mqttClient.connected()) {
    return;
  }
  
  char msg[250];
  snprintf(msg, 250, 
    "{\"device\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"free_heap\":%lu,\"reconnects\":%d}",
    DEVICE_NAME,
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI(),
    millis() / 1000,
    ESP.getFreeHeap(),
    mqttReconnectAttempts
  );
  
  mqttClient.publish(MQTT_TOPIC_LED_STATUS, msg);
  Serial.print("✓ Status published: ");
  Serial.println(msg);  
}

void publishData() {
  if (!mqttClient.connected()) {
    return;
  }
  
  messageCount++;
  
  char msg[350];
  
  snprintf(msg, 350,
    "{\"device\":\"%s\",\"count\":%d,\"uptime\":%lu,\"rssi\":%d,\"button\":%d,\"led\":{\"state\":\"%s\",\"r\":%d,\"g\":%d,\"b\":%d}}",
    DEVICE_NAME,
    messageCount,
    millis() / 1000,
    WiFi.RSSI(),
    digitalRead(PIN_BUTTON_ON_BOARD),
    ledState.isOn ? "on" : "off",
    ledState.red,
    ledState.green,
    ledState.blue
  );
  
  if (mqttClient.publish(MQTT_TOPIC_LED_STATUS, msg)) {
    Serial.print("✓ Data published (#");
    Serial.print(messageCount);
    Serial.println(")");
  } else {
    Serial.println("✗ Failed to publish data");
  }
}

// ========================================
// Hardware Setup
// ========================================
void setupHardware() {
  Serial.println("\n========================================");
  Serial.println("Hardware Initialization:");
  Serial.println("========================================");
  
  pinMode(PIN_BUTTON_ON_BOARD, INPUT_PULLUP);
  Serial.println("✓ Button (GPIO 4)");
  
  if (ENABLE_BUZZER) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("✓ Buzzer (GPIO 16)");
  }
  
  if (ENABLE_WS2812B) {
    strip.begin();
    strip.show();
    strip.setBrightness(LED_BRIGHTNESS);
    Serial.println("✓ WS2812B LED (GPIO 38)");
    
    // Default LED ON (white)
    setLED(true, 255, 255, 255);
    ledState.changed = false;
    Serial.println("✓ Default LED ON (white)");
  }
  
  Serial.println("========================================\n");
}

// ========================================
// Setup
// ========================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(2000);
  
  Serial.println("\n\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║        TOM NANDU SMART HOME            ║");
  Serial.println("║     REMOTE ACCESS OF YOUR HOME         ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.print("Chip: ");
  Serial.print(ESP.getChipModel());
  Serial.print(" (");
  Serial.print(ESP.getChipCores());
  Serial.println(" cores)");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / 1024 / 1024);
  Serial.println(" MB");
  Serial.println();
  
  setupHardware();
  setupWiFi();
  testNetworkConnectivity();
  testMQTTBrokerReachability();
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(15);
  
  Serial.println("✓ Setup complete! Starting main loop...\n");

  Serial.println("========================================");
  Serial.println("🧪 MQTT Monitoring Commands (run in terminal):");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t homeled/control -m \"red\" -- Will turn LED red");
  Serial.println("========================================");

  if (!MDNS.begin("tomfcb")) {
    Serial.println("❌ Error setting up MDNS");
  } else {
    Serial.println("✅ MDNS responder started");
    Serial.println("========================================");
    Serial.println("🌐 Access your ESP32 at: http://tomfcb.local");
    Serial.println("========================================");
    setupWebServer();
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);

    
  }
  if(PIN_BUTTON_ON_BOARD)
{
  
  digitalWrite(PIN_BUZZER, HIGH); 
  delay(500);
  digitalWrite(PIN_BUZZER, LOW);
}
}

// ========================================
// Main Loop
// ========================================
void loop() {
  // WiFi check
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi disconnected! Reconnecting...");
    setupWiFi();
  }
  
  // Handle web server requests
  server.handleClient();

  // MQTT connection with non-blocking reconnect
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnectMQTT();
    }
  } else {
    mqttClient.loop();
  }
  
  // Publish LED status on change
  if (ledState.changed && mqttClient.connected()) {
    publishLEDStatus();
  }

  if (digitalRead(PIN_BUTTON_ON_BOARD) == LOW) {  // Assuming button pulls LOW when pressed
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1000);
  digitalWrite(PIN_BUZZER, LOW);
}
  
}