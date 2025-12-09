#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"

// ============================================
// WIFI STATE (struct and array defined in config.h)
// ============================================
int currentWiFiIndex = 0;

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

struct WeatherData {
  float temperature;
  float windSpeed;
  int windDirection;
  int weatherCode;
  String condition;
  String windDir;
  String cityName;
  String lastUpdate;
  bool isValid;
} weatherData = {0.0, 0.0, 0, 0, "Unknown", "N/A", "Unknown", "", false};

// ============================================
// GLOBAL VARIABLES
// ============================================
unsigned long lastReconnectAttempt = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 120000;
int mqttReconnectAttempts = 0;
float latitude = 0.0;
float longitude = 0.0;

// ============================================
// FUNCTION DECLARATIONS
// ============================================
void setupWiFi();
void handleWiFiReconnection();
String getCurrentWiFiInfo();
void setupHardware();
void setupWebServer();
void setupTime();
void testMQTTConnection();
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
bool getLocationFromWiFi();
void getWeatherData();
String getWeatherDescription(int code);
String getWindDirection(int degrees);
String getCurrentTime();

// ============================================
// IMPROVED WIFI SETUP WITH FALLBACK
// ============================================
void setupWiFi() {
  delay(10);
  Serial.println("\n========================================");
  Serial.println("WiFi Connection with Fallback");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  for (int i = 0; i < NUM_WIFI_NETWORKS; i++) {
    Serial.printf("Attempt %d/%d - Trying: %s\n", i + 1, NUM_WIFI_NETWORKS, WIFI_NETWORKS[i].ssid);
    Serial.print("Connecting");
    
    WiFi.begin(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      currentWiFiIndex = i;
      Serial.println("✓ WiFi Connected Successfully!");
      Serial.printf("Network: %s\n", WIFI_NETWORKS[i].ssid);
      Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
      Serial.println("========================================\n");
      return;
    } else {
      Serial.printf("✗ Failed to connect to: %s\n", WIFI_NETWORKS[i].ssid);
      WiFi.disconnect();
      delay(1000);
    }
  }
  
  Serial.println("========================================");
  Serial.println("✗✗✗ ALL WiFi NETWORKS FAILED ✗✗✗");
  Serial.println("Available networks:");
  
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.printf("  %d: %s (%d dBm) %s\n", 
                  i + 1, 
                  WiFi.SSID(i).c_str(), 
                  WiFi.RSSI(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "[OPEN]" : "[SECURED]");
  }
  
  Serial.println("========================================");
  Serial.println("Restarting in 5 seconds...");
  delay(5000);
  ESP.restart();
}

// ============================================
// WIFI RECONNECTION WITH FALLBACK
// ============================================
void handleWiFiReconnection() {
  static unsigned long lastWiFiReconnectAttempt = 0;
  static int reconnectAttempts = 0;
  
  if (WiFi.status() == WL_CONNECTED) {
    reconnectAttempts = 0;
    return;
  }
  
  unsigned long now = millis();
  
  if (now - lastWiFiReconnectAttempt < 5000) {
    return;
  }
  
  lastWiFiReconnectAttempt = now;
  reconnectAttempts++;
  
  Serial.println("\n⚠️  WiFi Disconnected!");
  Serial.printf("Reconnection attempt %d\n", reconnectAttempts);
  
  if (reconnectAttempts <= 3) {
    Serial.printf("Quick reconnect to: %s\n", WIFI_NETWORKS[currentWiFiIndex].ssid);
    WiFi.reconnect();
    
    int wait = 0;
    while (WiFi.status() != WL_CONNECTED && wait < 6) {
      delay(500);
      Serial.print(".");
      wait++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✓ Reconnected successfully!");
      reconnectAttempts = 0;
      return;
    }
  }
  
  if (reconnectAttempts > 3) {
    Serial.println("Quick reconnect failed. Trying all networks...");
    WiFi.disconnect();
    delay(1000);
    setupWiFi();
    reconnectAttempts = 0;
  }
}

// ============================================
// GET CURRENT WIFI INFO
// ============================================
String getCurrentWiFiInfo() {
  if (WiFi.status() != WL_CONNECTED) {
    return "Not Connected";
  }
  
  String info = "";
  info += "SSID: " + String(WIFI_NETWORKS[currentWiFiIndex].ssid) + "\n";
  info += "IP: " + WiFi.localIP().toString() + "\n";
  info += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
  info += "Network: " + String(currentWiFiIndex + 1) + "/" + String(NUM_WIFI_NETWORKS);
  
  return info;
}

void setupTime() {
  Serial.println("🕐 Setting up time synchronization...");
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.print("Waiting for NTP time sync");
  int attempts = 0;
  while (time(nullptr) < 100000 && attempts < 20) {
    Serial.print(".");
    delay(500);
    attempts++;
  }
  Serial.println();
  
  if (time(nullptr) > 100000) {
    Serial.println("✓ Time synchronized!");
    Serial.print("Current time: ");
    Serial.println(getCurrentTime());
  } else {
    Serial.println("⚠️  Time sync failed");
  }
}

String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

bool getLocationFromWiFi() {
  Serial.println("📍 Fetching location from WiFi...");
  
  HTTPClient http;
  String url = "https://ipinfo.io/json";
  http.begin(url);
  http.setTimeout(10000);

  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("✗ JSON parsing failed");
      http.end();
      return false;
    }

    String loc = doc["loc"].as<String>();
    int commaIndex = loc.indexOf(',');
    
    if (commaIndex > 0) {
      latitude = loc.substring(0, commaIndex).toFloat();
      longitude = loc.substring(commaIndex + 1).toFloat();
      weatherData.cityName = doc["city"].as<String>();
      
      Serial.printf("✓ Location: %s\n", weatherData.cityName.c_str());
      Serial.printf("  Coordinates: %.4f, %.4f\n", latitude, longitude);
      
      http.end();
      return true;
    }
  } else {
    Serial.printf("✗ Geolocation API error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

void getWeatherData() {
  if (latitude == 0.0 && longitude == 0.0) {
    Serial.println("✗ No location data. Fetching location first...");
    if (!getLocationFromWiFi()) {
      Serial.println("✗ Failed to get location");
      return;
    }
  }

  Serial.println("\n🌤️  Fetching weather data...");
  
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" +
               String(latitude, 4) + "&longitude=" + String(longitude, 4) +
               "&current_weather=true&timezone=auto";

  http.begin(url);
  http.setTimeout(10000);

  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("✗ JSON parsing failed");
      http.end();
      return;
    }

    JsonObject current = doc["current_weather"];
    weatherData.temperature = current["temperature"];
    weatherData.windSpeed = current["windspeed"];
    weatherData.windDirection = current["winddirection"];
    weatherData.weatherCode = current["weathercode"];
    weatherData.condition = getWeatherDescription(weatherData.weatherCode);
    weatherData.windDir = getWindDirection(weatherData.windDirection);
    weatherData.lastUpdate = getCurrentTime();
    weatherData.isValid = true;

    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       WEATHER UPDATE                   ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Location:    %-26s║\n", weatherData.cityName.c_str());
    Serial.printf("║ Temperature: %.1f°C                     ║\n", weatherData.temperature);
    Serial.printf("║ Condition:   %-26s║\n", weatherData.condition.c_str());
    Serial.printf("║ Wind:        %.1f km/h (%s)            ║\n", weatherData.windSpeed, weatherData.windDir.c_str());
    Serial.println("╚════════════════════════════════════════╝\n");
    
    lastWeatherUpdate = millis();
    publishStatus();
  } else {
    Serial.printf("✗ HTTP error code: %d\n", httpCode);
    weatherData.isValid = false;
  }
  
  http.end();
}
String getWeatherDescription(int code) {
  switch(code) {
    case 0: return "Clear sky";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: return "Snow";
    case 77: return "Snow grains";
    case 80: case 81: case 82: return "Rain showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunderstorm with hail";
    default: return "Unknown";
  }
}

String getWindDirection(int degrees) {
  if (degrees >= 337.5 || degrees < 22.5) return "N";
  if (degrees >= 22.5 && degrees < 67.5) return "NE";
  if (degrees >= 67.5 && degrees < 112.5) return "E";
  if (degrees >= 112.5 && degrees < 157.5) return "SE";
  if (degrees >= 157.5 && degrees < 202.5) return "S";
  if (degrees >= 202.5 && degrees < 247.5) return "SW";
  if (degrees >= 247.5 && degrees < 292.5) return "W";
  if (degrees >= 292.5 && degrees < 337.5) return "NW";
  return "N/A";
}

void testMQTTConnection() {
  Serial.println("\n========================================");
  Serial.println("MQTT Connection Test");
  Serial.println("========================================");
  
  Serial.print("Testing DNS resolution for: ");
  Serial.println(MQTT_BROKER);
  
  IPAddress brokerIP;
  if (WiFi.hostByName(MQTT_BROKER, brokerIP)) {
    Serial.print("✓ DNS OK - Broker IP: ");
    Serial.println(brokerIP);
  } else {
    Serial.println("✗ DNS FAILED");
    return;
  }
  
  Serial.print("Testing TCP connection to port ");
  Serial.print(MQTT_PORT);
  Serial.print("... ");
  
  WiFiClient testClient;
  if (testClient.connect(MQTT_BROKER, MQTT_PORT)) {
    Serial.println("✓ TCP OK");
    testClient.stop();
  } else {
    Serial.println("✗ TCP FAILED");
    return;
  }
  
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
    Serial.println("✓ MQTT OK");
    mqttClient.disconnect();
  } else {
    Serial.print("✗ MQTT FAILED - Error: ");
    Serial.println(mqttClient.state());
  }
  
  Serial.println("========================================\n");
}

void setupHardware() {
  Serial.println("\n========================================");
  Serial.println("Hardware Initialization");
  Serial.println("========================================");
  
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  Serial.println("✓ Relays: Kitchen(10), Bedroom(11), Hall(12)");
  
  pinMode(PIN_BUTTON_ON_BOARD, INPUT_PULLUP);
  Serial.println("✓ Button (GPIO 4)");
  
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("✓ Buzzer (GPIO 16)");
  
  strip.begin();
  strip.show();
  strip.setBrightness(LED_BRIGHTNESS);
  Serial.println("✓ WS2812B LED (GPIO 38)");
  
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

void handleLightControl(String command) {
  command.trim();
  command.toLowerCase();
  
  Serial.print("🎛️  Processing: ");
  Serial.println(command);
  
  if (command == "kitchen_light_on") setLight(1, true);
  else if (command == "kitchen_light_off") setLight(1, false);
  else if (command == "bed1_light_on") setLight(2, true);
  else if (command == "bed1_light_off") setLight(2, false);
  else if (command == "living_light_on") setLight(3, true);
  else if (command == "living_light_off") setLight(3, false);
  else if (command == "led_on") setLED(true);
  else if (command == "led_off") setLED(false);
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
  else if (command == "buzzer_on") setBuzzer(true);
  else if (command == "weather_update") getWeatherData();
  else if (command == "status") publishStatus();
  else Serial.println("⚠️  Unknown command");
}

void handleLEDControl(String command) {
  Serial.print("💡 LED: ");
  Serial.println(command);
  
  command.toLowerCase();
  
  if (command == "on") setLED(true);
  else if (command == "off") setLED(false);
  else Serial.println("⚠️  Unknown LED command");
}

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
  digitalWrite(pin, state ? LOW : HIGH);
  
  Serial.printf("💡 %s %s\n", roomName, state ? "ON ✓" : "OFF");
  
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

void setLED(bool state) {
  ledState.isOn = state;
  
  if (state) {
    strip.setBrightness(ledState.brightness);
    strip.setPixelColor(0, strip.Color(255, 255, 255));
    strip.show();
    Serial.println("✓ LED ON");
  } else {
    strip.clear();
    strip.show();
    Serial.println("✓ LED OFF");
  }
}

void setBuzzer(bool state) {
  if (state) {
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println("🔔 Buzzer ON");
    delay(500);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("🔔 Buzzer OFF");
  }
}

void publishStatus() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<768> doc;
  doc["kitchen_light"] = deviceState.kitchenLight ? "on" : "off";
  doc["bed1_light"] = deviceState.bedroomLight ? "on" : "off";
  doc["living_light"] = deviceState.hallLight ? "on" : "off";
  doc["led"] = ledState.isOn ? "on" : "off";
  doc["buzzer"] = deviceState.buzzer ? "on" : "off";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["wifi_ssid"] = WIFI_NETWORKS[currentWiFiIndex].ssid;
  doc["wifi_network"] = String(currentWiFiIndex + 1) + "/" + String(NUM_WIFI_NETWORKS);
  
  if (weatherData.isValid) {
    doc["temperature"] = weatherData.temperature;
    doc["weather_condition"] = weatherData.condition;
    doc["wind_speed"] = weatherData.windSpeed;
    doc["wind_direction"] = weatherData.windDir;
    doc["city"] = weatherData.cityName;
    doc["weather_update"] = weatherData.lastUpdate;
  }
  
  char buffer[768];
  serializeJson(doc, buffer);
  
  if (mqttClient.publish(MQTT_TOPIC_STATUS, buffer, true)) {
    Serial.print("📤 Status: ");
    Serial.println(buffer);
  }
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
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
  StaticJsonDocument<768> doc;
  doc["kitchen"] = deviceState.kitchenLight;
  doc["bedroom"] = deviceState.bedroomLight;
  doc["hall"] = deviceState.hallLight;
  doc["led"] = ledState.isOn;
  doc["ip"] = WiFi.localIP().toString();
  doc["mqtt"] = mqttClient.connected() ? "Connected" : "Disconnected";
  doc["rssi"] = WiFi.RSSI();
  doc["wifi_ssid"] = WIFI_NETWORKS[currentWiFiIndex].ssid;
  doc["wifi_network"] = String(currentWiFiIndex + 1) + "/" + String(NUM_WIFI_NETWORKS);
  
  if (weatherData.isValid) {
    doc["temperature"] = weatherData.temperature;
    doc["weather"] = weatherData.condition;
    doc["wind_speed"] = weatherData.windSpeed;
    doc["wind_direction"] = weatherData.windDir;
    doc["city"] = weatherData.cityName;
    doc["weather_update"] = weatherData.lastUpdate;
  }
  
  char buffer[768];
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

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  SMART HOME + WEATHER v4.0             ║");
  Serial.println("║  Multi-WiFi Fallback Support           ║");
  Serial.println("║  Kitchen + Bedroom + Hall + Weather    ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  setupHardware();
  setupWiFi();
  setupTime();
  testMQTTConnection();
  setupWebServer();
  
  if (MDNS.begin("smarthome")) {
    Serial.println("✓ MDNS: http://smarthome.local");
  }
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(768);
  mqttClient.setKeepAlive(60);
  
  if (getLocationFromWiFi()) {
    delay(2000);
    getWeatherData();
  }
  
  Serial.println("\n========================================");
  Serial.println("📋 MQTT Commands:");
  Serial.println("========================================");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"kitchen_light_on\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"all_off\"");
  Serial.println("mosquitto_pub -h broker.hivemq.com -t smarthome/control -m \"weather_update\"");
  Serial.println("========================================\n");
  
  randomSeed(micros());
}

void loop() {
  server.handleClient();
  
  handleWiFiReconnection();
  
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnectMQTT()) {                                    
        lastReconnectAttempt = 0;
      }
    }
  } else {            
    mqttClient.loop();
  }   

  unsigned long now = millis();   
  if (now - lastStatusPublish > 60000) {
    lastStatusPublish = now;
    publishStatus();
  }                               
  if (now - lastWeatherUpdate > weatherUpdateInterval) {
    lastWeatherUpdate = now;
    getWeatherData();
  }
}
