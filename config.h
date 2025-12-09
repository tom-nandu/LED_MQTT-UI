#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// SERIAL CONFIGURATION
// ============================================
#define SERIAL_BAUD_RATE 115200

// ============================================
// WIFI CONFIGURATION
// ============================================
/ Primary WiFi
#define WIFI_SSID_1 "Charlie"
#define WIFI_PASS_1 "logu9769"

// Secondary WiFi (Fallback)
#define WIFI_SSID_2 "tom"
#define WIFI_PASS_2 "tttttttt"

// Keep these for backward compatibility
#define WIFI_SSID WIFI_SSID_1
#define WIFI_PASS WIFI_PASS_1

// ============================================
// MQTT CONFIGURATION (Matching Dashboard)
// ============================================
#define MQTT_BROKER "broker.hivemq.com"  // Free public MQTT broker
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP32_SmartHome_"
#define MQTT_USERNAME ""  // Leave empty for public broker
#define MQTT_PASSWORD ""  // Leave empty for public broker

// ============================================
// MQTT TOPICS (Matching Dashboard)
// ============================================
#define MQTT_TOPIC_CONTROL "smarthome/control"
#define MQTT_TOPIC_LED_CONTROL "smarthome/led/control"
#define MQTT_TOPIC_STATUS "smarthome/status"

// ============================================
// PIN DEFINITIONS (Based on your hardware)
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
#define NUM_LEDS 1  // Single WS2812B LED
#define LED_BRIGHTNESS 50  // 0-255

// ============================================
// TIMING CONFIGURATION
// ============================================
#define STATUS_PUBLISH_INTERVAL 60000  // 60 seconds in milliseconds

// ============================================
// WEB SERVER HTML PAGE (Minimal - Dashboard is separate)
// ============================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>ESP32 Smart Home</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:'Segoe UI',sans-serif}
body{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;align-items:center;justify-content:center;color:#fff}
.container{max-width:500px;background:rgba(255,255,255,0.1);backdrop-filter:blur(20px);border-radius:20px;padding:2rem;box-shadow:0 20px 60px rgba(0,0,0,0.3)}
h1{font-size:2rem;margin-bottom:1rem;text-align:center}
.info{background:rgba(255,255,255,0.05);padding:1rem;border-radius:10px;margin-bottom:1rem}
.info p{margin:0.5rem 0;font-size:0.9rem}
.status{display:inline-block;width:10px;height:10px;border-radius:50%;background:#22d18b;margin-right:8px;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.link{display:block;text-align:center;padding:1rem;background:linear-gradient(135deg,#22d18b,#00b894);color:#fff;text-decoration:none;border-radius:10px;font-weight:600;margin-top:1rem;transition:transform 0.2s}
.link:hover{transform:translateY(-2px)}
</style>
</head>
<body>
<div class="container">
<h1>🏠 ESP32 Smart Home</h1>
<div class="info">
<p><span class="status"></span>Device Online</p>
<p>IP: <strong id="ip">Loading...</strong></p>
<p>MQTT: <strong id="mqtt">Checking...</strong></p>
<p>Uptime: <strong id="uptime">0s</strong></p>
</div>
<a href="/status" class="link">📊 View Status JSON</a>
<div style="margin-top:1rem;padding:1rem;background:rgba(0,255,255,0.1);border-radius:10px;font-size:0.85rem">
<strong>🌐 Access Full Dashboard:</strong><br>
Open <strong>index.html</strong> (login page) or <strong>dashboard.html</strong> from your computer/phone browser
</div>
</div>
<script>
fetch('/status').then(r=>r.json()).then(d=>{
document.getElementById('ip').textContent=d.ip;
document.getElementById('mqtt').textContent=d.mqtt;
document.getElementById('uptime').textContent=d.uptime+'s';
});
</script>
</body>
</html>
)rawliteral";

#endif // CONFIG_H
