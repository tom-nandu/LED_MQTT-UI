#include <WebServer.h>
#include "ledserver.h"
#include <Arduino.h>
#include <WiFi.h>

// LED state from main.cpp needs to be accessible
extern struct LEDState {
  bool isOn;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  bool changed;
} ledState;

// IMPORTANT: Import setLED function from main.cpp
extern void setLED(bool state, uint8_t r, uint8_t g, uint8_t b);

// Create the WebServer instance
WebServer server(80);

// Handler for root page
void handleRoot() {
  Serial.println("[WEB] Root page requested");
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 LED Control</title>
  <style>
    body { font-family: Arial; text-align: center; margin: 20px; background: #1a1a1a; color: white; }
    .button { 
      padding: 15px 30px; 
      margin: 10px; 
      font-size: 18px; 
      cursor: pointer; 
      border-radius: 5px;
      border: none;
      transition: transform 0.1s;
    }
    .button:active { transform: scale(0.95); }
    .on { background-color: #4CAF50; color: white; }
    .off { background-color: #f44336; color: white; }
    .color { background-color: #2196F3; color: white; }
    #status { 
      margin-top: 20px; 
      font-size: 16px; 
      padding: 10px; 
      background: #333; 
      border-radius: 5px;
      min-height: 30px;
    }
    #ledPreview { 
      width: 100px; 
      height: 100px; 
      margin: 20px auto; 
      border-radius: 50%; 
      border: 3px solid #666;
      transition: all 0.3s;
      background: #333;
    }
    .led-info {
      margin-top: 10px;
      font-size: 14px;
      color: #aaa;
    }
  </style>
</head>
<body>
  <h1>🏠 ESP32 LED Control</h1>
  <div id="ledPreview"></div>
  <div class="led-info">
    <div>State: <span id="ledStateText">Loading...</span></div>
    <div>RGB: <span id="ledRGB">-</span></div>
  </div>
  <div>
    <button class="button on" onclick="ledControl('on')">LED ON</button>
    <button class="button off" onclick="ledControl('off')">LED OFF</button>
  </div>
  <div>
    <button class="button color" onclick="ledControl('red')">Red</button>
    <button class="button color" onclick="ledControl('green')">Green</button>
    <button class="button color" onclick="ledControl('blue')">Blue</button>
    <button class="button color" onclick="ledControl('white')">White</button>
  </div>
  <div>
    <button class="button color" onclick="ledControl('yellow')">Yellow</button>
    <button class="button color" onclick="ledControl('cyan')">Cyan</button>
    <button class="button color" onclick="ledControl('magenta')">Magenta</button>
  </div>
  <div id="status">Click a button to control LED</div>

  <script>
    function ledControl(command) {
      console.log('Button clicked:', command);
      document.getElementById('status').innerHTML = '⏳ Sending: ' + command + '...';
      
      fetch('/led/' + command)
        .then(response => {
          console.log('Response status:', response.status);
          if (!response.ok) throw new Error('HTTP ' + response.status);
          return response.text();
        })
        .then(data => {
          console.log('Response data:', data);
          document.getElementById('status').innerHTML = '✓ ' + data;
          // Immediately update status after command
          setTimeout(updateStatus, 100);
        })
        .catch(error => {
          console.error('Fetch error:', error);
          document.getElementById('status').innerHTML = '✗ Error: ' + error;
        });
    }

    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          console.log('LED Status:', data);
          
          // Update LED preview
          let preview = document.getElementById('ledPreview');
          let stateText = document.getElementById('ledStateText');
          let rgbText = document.getElementById('ledRGB');
          
          if (data.state === 'on') {
            preview.style.backgroundColor = 'rgb(' + data.red + ',' + data.green + ',' + data.blue + ')';
            preview.style.boxShadow = '0 0 30px rgba(' + data.red + ',' + data.green + ',' + data.blue + ', 0.8)';
            stateText.textContent = 'ON';
            stateText.style.color = '#4CAF50';
          } else {
            preview.style.backgroundColor = '#333';
            preview.style.boxShadow = 'none';
            stateText.textContent = 'OFF';
            stateText.style.color = '#f44336';
          }
          
          rgbText.textContent = '(' + data.red + ', ' + data.green + ', ' + data.blue + ')';
        })
        .catch(error => console.error('Status error:', error));
    }

    // Update status every 1 second
    setInterval(updateStatus, 1000);
    // Initial update
    updateStatus();
    
    console.log('LED Control UI loaded successfully');
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
  Serial.println("[WEB] Root page sent");
}

// LED control handlers - NOW USING setLED() function!
void handleLEDOn() {
  //Serial.println("[WEB] ===== LED ON command received =====");
  if (ledState.red == 0 && ledState.green == 0 && ledState.blue == 0) {
    setLED(true, 255, 255, 255);  // Use setLED instead of direct assignment
  } else {
    setLED(true, ledState.red, ledState.green, ledState.blue);
  }
  server.send(200, "text/plain", "LED turned ON");
  //Serial.println("[WEB] Response sent");
}

void handleLEDOff() {
  //Serial.println("[WEB] ===== LED OFF command received =====");
  setLED(false, 0, 0, 0);  // Use setLED instead of direct assignment
  server.send(200, "text/plain", "LED turned OFF");
  //Serial.println("[WEB] Response sent");
}

void handleRed() {
  //Serial.println("[WEB] ===== RED command received =====");
  setLED(true, 255, 0, 0);  // Use setLED
  server.send(200, "text/plain", "LED set to RED");
  //Serial.println("[WEB] Response sent");
}

void handleGreen() {
  //Serial.println("[WEB] ===== GREEN command received =====");
  setLED(true, 0, 255, 0);  // Use setLED
  server.send(200, "text/plain", "LED set to GREEN");
  //Serial.println("[WEB] Response sent");
}

void handleBlue() {
  //Serial.println("[WEB] ===== BLUE command received =====");
  setLED(true, 0, 0, 255);  // Use setLED
  server.send(200, "text/plain", "LED set to BLUE");
  //Serial.println("[WEB] Response sent");
}

void handleWhite() {
  //Serial.println("[WEB] ===== WHITE command received =====");
  setLED(true, 255, 255, 255);  // Use setLED
  server.send(200, "text/plain", "LED set to WHITE");
  //Serial.println("[WEB] Response sent");
}

void handleYellow() {
  //Serial.println("[WEB] ===== YELLOW command received =====");
  setLED(true, 255, 255, 0);  // Use setLED
  server.send(200, "text/plain", "LED set to YELLOW");
  //Serial.println("[WEB] Response sent");
}

void handleCyan() {
  //Serial.println("[WEB] ===== CYAN command received =====");
  setLED(true, 0, 255, 255);  // Use setLED
  server.send(200, "text/plain", "LED set to CYAN");
  //Serial.println("[WEB] Response sent");
}

void handleMagenta() {
  //Serial.println("[WEB] ===== MAGENTA command received =====");
  setLED(true, 255, 0, 255);  // Use setLED
  server.send(200, "text/plain", "LED set to MAGENTA");
  //Serial.println("[WEB] Response sent");
}

// Status endpoint
void handleStatus() {
  String json = "{";
  json += "\"state\":\"" + String(ledState.isOn ? "on" : "off") + "\",";
  json += "\"red\":" + String(ledState.red) + ",";
  json += "\"green\":" + String(ledState.green) + ",";
  json += "\"blue\":" + String(ledState.blue) + ",";
  json += "\"brightness\":" + String(ledState.brightness);
  json += "}";
  
  server.send(200, "application/json", json);
  // Removed log to reduce spam
}

void handleNotFound() {
  Serial.print("[WEB] 404 - Not Found: ");
  Serial.println(server.uri());
  server.send(404, "text/plain", "404: Not Found - " + server.uri());
}

// Setup function to initialize web server
void setupWebServer() {
  Serial.println("\n========================================");
  Serial.println("Setting up Web Server Routes:");
  Serial.println("========================================");
  
  // Register all routes
  server.on("/", handleRoot);
  //Serial.println("✓ Route: /");
  
  server.on("/led/on", handleLEDOn);
  //Serial.println("✓ Route: /led/on");
  
  server.on("/led/off", handleLEDOff);
  //Serial.println("✓ Route: /led/off");
  
  server.on("/led/red", handleRed);
  //Serial.println("✓ Route: /led/red");
  
  server.on("/led/green", handleGreen);
  //Serial.println("✓ Route: /led/green");
  
  server.on("/led/blue", handleBlue);
  //Serial.println("✓ Route: /led/blue");
  
  server.on("/led/white", handleWhite);
  //Serial.println("✓ Route: /led/white");
  
  server.on("/led/yellow", handleYellow);
  //Serial.println("✓ Route: /led/yellow");
  
  server.on("/led/cyan", handleCyan);
  //Serial.println("✓ Route: /led/cyan");
  
  server.on("/led/magenta", handleMagenta);
  //Serial.println("✓ Route: /led/magenta");
  
  server.on("/status", handleStatus);
  //Serial.println("✓ Route: /status");
  
  server.onNotFound(handleNotFound);
  //Serial.println("✓ 404 handler registered");
  
  // Start server
  server.begin();
  Serial.println("========================================");
  Serial.println("✓ Web Server started successfully");
  Serial.print("✓ Access at: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.println("========================================\n");
}