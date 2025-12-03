#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================
// PIN DEFINITIONS
// ============================================
#define PIN_BUTTON 4
#define PIN_BUZZER 16

// ============================================
// CONFIGURATION
// ============================================
const char* ssid = "ACT-ai_102757697732";
const char* password = "18788147";

// ============================================
// GLOBAL VARIABLES
// ============================================
float latitude = 0.0;
float longitude = 0.0;
String cityName = "";
String timezoneStr = "Asia/Kolkata";
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 120000; // 2 minutes auto-update

// Button debouncing
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ============================================
// FUNCTION DECLARATIONS
// ============================================
void setupWiFi();
void setupHardware();
void setupTime();
String getCurrentTime();
bool getLocationFromWiFi();
void getWeatherData();
String getWeatherDescription(int code);
String getWindDirection(int degrees);

// ============================================
// WIFI SETUP
// ============================================
void setupWiFi() {
  delay(10);
  Serial.println("\n========================================");
  Serial.println("WiFi Connection");
  Serial.println("========================================");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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
  
  // Button
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  Serial.println("✓ Button configured on GPIO 4");
  
  // Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("✓ Buzzer configured on GPIO 16");
  
  // Welcome beep
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  
  Serial.println("========================================\n");
}

// ============================================
// TIME SETUP (NTP)
// ============================================
void setupTime() {
  Serial.println("🕐 Setting up time synchronization...");
  
  // Configure NTP with multiple servers for reliability
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  // 19800 = GMT+5:30 for India (5.5 hours * 3600 seconds)
  
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
    Serial.println("⚠️  Time sync failed, will show API time");
  }
}

// ============================================
// GET CURRENT TIME
// ============================================
String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

// ============================================
// GET LOCATION FROM WIFI (IP-BASED)
// ============================================
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

    String loc = doc["loc"].as<String>(); // "lat,lon"
    int commaIndex = loc.indexOf(',');
    
    if (commaIndex > 0) {
      latitude = loc.substring(0, commaIndex).toFloat();
      longitude = loc.substring(commaIndex + 1).toFloat();
      cityName = doc["city"].as<String>();
      
      Serial.printf("✓ Location: %s\n", cityName.c_str());
      Serial.printf("  Region: %s\n", doc["region"].as<const char*>());
      Serial.printf("  Country: %s\n", doc["country"].as<const char*>());
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

// ============================================
// GET WEATHER DATA
// ============================================
void getWeatherData() {
  if (latitude == 0.0 && longitude == 0.0) {
    Serial.println("✗ No location data available. Fetching location first...");
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
    float temp = current["temperature"];
    float windSpeed = current["windspeed"];
    int windDirection = current["winddirection"];
    int weatherCode = current["weathercode"];
    const char* apiTime = current["time"];

    // Success beep
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);
    delay(50);
    digitalWrite(PIN_BUZZER, HIGH);
    delay(100);
    digitalWrite(PIN_BUZZER, LOW);

    // Get current actual time
    String currentTime = getCurrentTime();

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       CURRENT WEATHER UPDATE           ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Location:    %-26s║\n", cityName.c_str());
    Serial.printf("║ Fetched at:  %-26s║\n", currentTime.c_str());
    Serial.printf("║ Data from:   %-26s║\n", apiTime);
    Serial.printf("║ Temperature: %.1f°C                     ║\n", temp);
    Serial.printf("║ Condition:   %-26s║\n", getWeatherDescription(weatherCode).c_str());
    Serial.printf("║ Wind Speed:  %.1f km/h                  ║\n", windSpeed);
    Serial.printf("║ Wind Dir:    %d° (%s)                  ║\n", windDirection, getWindDirection(windDirection).c_str());
    Serial.println("╚════════════════════════════════════════╝\n");
    
    lastUpdate = millis();
  } else {
    Serial.printf("✗ HTTP error code: %d\n", httpCode);
    // Error beep
    digitalWrite(PIN_BUZZER, HIGH);
    delay(500);
    digitalWrite(PIN_BUZZER, LOW);
  }
  
  http.end();
}

// ============================================
// WEATHER CODE TO DESCRIPTION
// ============================================
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

// ============================================
// WIND DIRECTION
// ============================================
String getWindDirection(int degrees) {
  if (degrees >= 337.5 || degrees < 22.5) return "N";
  if (degrees >= 22.5 && degrees < 67.5) return "NE";
  if (degrees >= 67.5 && degrees < 112.5) return "E";
  if (degrees >= 112.5 && degrees < 157.5) return "SE";
  if (degrees >= 157.5 && degrees < 202.5) return "S";
  if (degrees >= 202.5 && degrees < 247.5) return "SW";
  if (degrees >= 247.5 && degrees < 292.5) return "W";
  if (degrees >= 292.5 && degrees < 337.5) return "NW";
  return "Unknown";
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   ESP32 WEATHER STATION v1.0           ║");
  Serial.println("║   Press Button for Weather Update      ║");
  Serial.println("╚════════════════════════════════════════╝");
  
  setupHardware();
  setupWiFi();
  
  // Setup time synchronization
  setupTime();
  
  // Get initial location
  if (getLocationFromWiFi()) {
    Serial.println("\n💡 Press button on GPIO 4 to get weather data");
    Serial.println("   Auto-update every 2 minutes\n");
  } else {
    Serial.println("⚠️  Failed to get location. Press button to retry.\n");
  }
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // WiFi check
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi lost! Reconnecting...");
    setupWiFi();
  }
  
  // Button control with debouncing
  int buttonReading = digitalRead(PIN_BUTTON);
  
  if (buttonReading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool lastStableState = HIGH;
    
    if (buttonReading != lastStableState) {
      lastStableState = buttonReading;
      
      if (buttonReading == LOW) {
        // Button pressed
        Serial.println("\n🔘 Button pressed! Fetching weather...");
        getWeatherData();
      }
    }
  }
  
  lastButtonState = buttonReading;
  
  // Auto-update every 2 minutes
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval && latitude != 0.0) {
    Serial.println("\n⏰ Auto-update (2 min) - Fetching weather data...");
    getWeatherData();
  }
  
  delay(10);
}
