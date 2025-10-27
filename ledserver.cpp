#include <WebServer.h>
#include "ledserver.h"
#include <Arduino.h>
#include <WiFi.h>
#include "user_roles.h"
#include "pins.h"

extern struct LEDState {
  bool isOn;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t brightness;
  bool changed;
} ledState;

extern void setLED(bool state, uint8_t r, uint8_t g, uint8_t b);

WebServer server(80);

struct ActivityLog {
  String timestamp;
  String username;
  String action;
};

ActivityLog activityLogs[50];
int logIndex = 0;

struct Session {
  String token;
  String username;
  UserRole role;
  unsigned long loginTime;
};

Session activeSessions[10];
const unsigned long SESSION_TIMEOUT = 3600000;

// Forward declarations - MUST be after struct definitions
Session* getSessionFromRequest();
Session* validateSession(String token);
String createSession(String username, UserRole role);
String generateToken();
void addLog(String username, String action);
void handleDashboard();
void handleRoot();
void handleLogin();

void addLog(String username, String action) {
  activityLogs[logIndex].timestamp = String(millis() / 1000) + "s";
  activityLogs[logIndex].username = username;
  activityLogs[logIndex].action = action;
  logIndex = (logIndex + 1) % 50;
  Serial.println("[LOG] " + username + ": " + action);
}

String generateToken() {
  String token = "";
  for (int i = 0; i < 32; i++) {
    token += String(random(0, 16), HEX);
  }
  return token;
}

String createSession(String username, UserRole role) {
  String token = generateToken();
  int slot = 0;
  unsigned long oldestTime = activeSessions[0].loginTime;
  
  for (int i = 0; i < 10; i++) {
    if (activeSessions[i].token == "") {
      slot = i;
      break;
    }
    if (activeSessions[i].loginTime < oldestTime) {
      oldestTime = activeSessions[i].loginTime;
      slot = i;
    }
  }
  
  activeSessions[slot].token = token;
  activeSessions[slot].username = username;
  activeSessions[slot].role = role;
  activeSessions[slot].loginTime = millis();
  
  Serial.println("[SESSION] Created for " + username + " - Token: " + token.substring(0, 8) + "...");
  return token;
}

Session* validateSession(String token) {
  Serial.println("[SESSION] Validating token: " + token.substring(0, 8) + "...");
  
  for (int i = 0; i < 10; i++) {
    if (activeSessions[i].token.length() > 0) {
      Serial.println("[SESSION] Slot " + String(i) + " token: " + activeSessions[i].token.substring(0, 8) + "... User: " + activeSessions[i].username);
      
      if (activeSessions[i].token == token) {
        Serial.println("[SESSION] Token match found in slot " + String(i));
        
        if (millis() - activeSessions[i].loginTime > SESSION_TIMEOUT) {
          Serial.println("[SESSION] Token expired");
          activeSessions[i].token = "";
          return nullptr;
        }
        
        Serial.println("[SESSION] ✓ Token valid for: " + activeSessions[i].username);
        return &activeSessions[i];
      }
    }
  }
  
  Serial.println("[SESSION] ✗ Token not found in any slot");
  return nullptr;
}

Session* getSessionFromRequest() {
  Serial.println("[SESSION] === Checking for session cookie ===");
  
  // Print all headers for debugging
  Serial.println("[SESSION] All headers:");
  for (int i = 0; i < server.headers(); i++) {
    Serial.println("  " + server.headerName(i) + ": " + server.header(i));
  }
  
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    Serial.println("[SESSION] Cookie header found: " + cookie);
    
    int tokenStart = cookie.indexOf("session=");
    if (tokenStart != -1) {
      tokenStart += 8; // Length of "session="
      int tokenEnd = cookie.indexOf(";", tokenStart);
      String token;
      
      if (tokenEnd == -1) {
        token = cookie.substring(tokenStart);
      } else {
        token = cookie.substring(tokenStart, tokenEnd);
      }
      
      // Remove any whitespace from token
      token.trim();
      
      Serial.println("[SESSION] Extracted token: " + token);
      Serial.println("[SESSION] Token length: " + String(token.length()));
      
      Session* session = validateSession(token);
      
      if (session != nullptr) {
        Serial.println("[SESSION] ✓ Valid session for: " + session->username);
      } else {
        Serial.println("[SESSION] ✗ Session validation failed");
      }
      
      return session;
    } else {
      Serial.println("[SESSION] ✗ No 'session=' found in cookie");
      Serial.println("[SESSION] Cookie contents: " + cookie);
    }
  } else {
    Serial.println("[SESSION] ✗ No Cookie header present");
  }
  
  Serial.println("[SESSION] === Session check complete: NULL ===");
  return nullptr;
}

void handleRoot() {
  Serial.println("[WEB] Root page requested");
  Session* session = getSessionFromRequest();
  
  if (session != nullptr) {
    Serial.println("[WEB] User already logged in, redirecting to dashboard");
    handleDashboard();
    return;
  }

  String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 LED - Login</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .login-container {
      background: white;
      padding: 40px;
      border-radius: 15px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      width: 100%;
      max-width: 450px;
    }
    h1 { color: #333; margin-bottom: 10px; text-align: center; }
    .subtitle { color: #666; text-align: center; margin-bottom: 30px; font-size: 14px; }
    .form-group { margin-bottom: 20px; }
    label { display: block; margin-bottom: 5px; color: #555; font-weight: 500; }
    input {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      transition: border-color 0.3s;
    }
    input:focus { outline: none; border-color: #667eea; }
    .button {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s, box-shadow 0.2s;
    }
    .button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
    }
    .button:active { transform: translateY(0); }
    #message {
      margin-top: 15px;
      padding: 12px;
      border-radius: 8px;
      text-align: center;
      display: none;
    }
    .error { background: #fee; color: #c33; display: block; }
    .success { background: #efe; color: #3c3; display: block; }
    .credentials {
      margin-top: 20px;
      padding: 15px;
      background: #f5f5f5;
      border-radius: 8px;
      font-size: 12px;
      color: #666;
    }
    .credentials strong { color: #333; display: block; margin-bottom: 8px; }
    .cred-row { 
      padding: 6px 0; 
      border-bottom: 1px solid #e0e0e0;
    }
    .cred-row:last-child { border-bottom: none; }
    .role-badge {
      display: inline-block;
      padding: 2px 8px;
      border-radius: 10px;
      font-size: 10px;
      font-weight: bold;
      margin-left: 8px;
    }
    .badge-admin { background: #ff5722; color: white; }
    .badge-mod { background: #ff9800; color: white; }
    .badge-viewer { background: #2196F3; color: white; }
    .badge-guest { background: #9e9e9e; color: white; }
  </style>
</head>
<body>
  <div class="login-container">
    <h1>🔐 ESP32 LED Control</h1>
    <p class="subtitle">Role-Based Access Control System</p>
    
    <form id="loginForm" onsubmit="return handleLogin(event)">
      <div class="form-group">
        <label for="username">Username</label>
        <input type="text" id="username" name="username" required autocomplete="username">
      </div>
      <div class="form-group">
        <label for="password">Password</label>
        <input type="password" id="password" name="password" required autocomplete="current-password">
      </div>
      <button type="submit" class="button">Login</button>
    </form>
    
    <div id="message"></div>
    
    <div class="credentials">
      <strong>🔑 Test Accounts & Permissions:</strong>
      <div class="cred-row">
        <span class="role-badge badge-admin">ADMIN</span> admin / admin123<br>
        <small style="color: #888;">✓ Full Control + Settings</small>
      </div>
      <div class="cred-row">
        <span class="role-badge badge-mod">MOD</span> moderator / mod123<br>
        <small style="color: #888;">✓ LED Control + View Logs</small>
      </div>
      <div class="cred-row">
        <span class="role-badge badge-viewer">VIEW</span> viewer / view123<br>
        <small style="color: #888;">✓ View Status + Logs Only</small>
      </div>
      <div class="cred-row">
        <span class="role-badge badge-guest">GUEST</span> guest / guest123<br>
        <small style="color: #888;">✓ View Status Only</small>
      </div>
    </div>
  </div>

  <script>
    function handleLogin(e) {
      e.preventDefault();
      
      const username = document.getElementById('username').value;
      const password = document.getElementById('password').value;
      const message = document.getElementById('message');
      
      console.log('=== LOGIN ATTEMPT ===');
      console.log('Username:', username);
      console.log('Password length:', password.length);
      
      message.textContent = '⏳ Authenticating...';
      message.className = '';
      message.style.display = 'block';
      
      const formData = 'username=' + encodeURIComponent(username) + '&password=' + encodeURIComponent(password);
      console.log('Sending POST to /login');
      
      fetch('/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData,
        credentials: 'include'
      })
      .then(response => {
        console.log('Response received');
        console.log('Status:', response.status);
        
        console.log('Headers:');
        response.headers.forEach((value, key) => {
          console.log('  ' + key + ':', value);
        });
        
        // Check if Set-Cookie header is present
        const setCookie = response.headers.get('set-cookie');
        console.log('Set-Cookie header:', setCookie);
        
        if (!response.ok) {
          throw new Error('HTTP ' + response.status);
        }
        
        return response.json();
      })
      .then(data => {
        console.log('Response data:', data);
        
        if (data.success) {
          console.log('✓ Login successful!');
          console.log('Token (partial):', data.token);
          message.textContent = '✓ Login successful! Loading dashboard...';
          message.className = 'success';
          
          // Check if cookies are enabled
          document.cookie = "test=1";
          const cookiesEnabled = document.cookie.indexOf("test=") !== -1;
          console.log('Cookies enabled:', cookiesEnabled);
          
          // Increased timeout to 1500ms for better reliability
          console.log('Waiting 1500ms for cookie to be set...');
          setTimeout(function() {
            console.log('Current cookies:', document.cookie);
            console.log('Redirecting to /dashboard now...');
            window.location.href = '/dashboard';
          }, 1500);
        } else {
          console.log('✗ Login failed:', data.message);
          message.textContent = '✗ ' + (data.message || 'Invalid credentials');
          message.className = 'error';
        }
      })
      .catch(error => {
        console.error('Login error:', error);
        message.textContent = '✗ Connection error: ' + error.message;
        message.className = 'error';
      });
      
      return false;
    }
    
    console.log('Login page loaded successfully');
    console.log('Cookies enabled:', navigator.cookieEnabled);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
  Serial.println("[WEB] Login page sent");
}

void handleLogin() {
  Serial.println("\n[WEB] ========== LOGIN REQUEST ==========");
  Serial.println("[WEB] Method: " + String(server.method() == HTTP_POST ? "POST" : "GET"));
  Serial.println("[WEB] URI: " + server.uri());
  
  if (!server.hasArg("username") || !server.hasArg("password")) {
    Serial.println("[WEB] ERROR: Missing credentials");
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing credentials\"}");
    return;
  }
  
  String username = server.arg("username");
  String password = server.arg("password");
  
  Serial.println("[WEB] Username: " + username);
  Serial.println("[WEB] Password length: " + String(password.length()));
  Serial.println("[WEB] Attempting authentication...");
  
  User* user = authenticateUser(username, password);
  
  if (user != nullptr) {
    Serial.println("[WEB] ✓ Authentication SUCCESSFUL!");
    Serial.println("[WEB] User role: " + String(getRoleName(user->role)));
    
    String token = createSession(username, user->role);
    Serial.println("[WEB] Token created: " + token.substring(0, 8) + "...");
    Serial.println("[WEB] Full token: " + token);
    
    addLog(username, "Logged in as " + String(getRoleName(user->role)));
    
    // FIXED: Use space after semicolons (HTTP standard) and remove SameSite for compatibility
    String cookie = "session=" + token + "; Path=/; Max-Age=3600; HttpOnly";
    
    Serial.println("[WEB] Setting cookie: " + cookie);
    
    server.sendHeader("Set-Cookie", cookie);
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    
    String response = "{\"success\":true,\"role\":\"" + String(getRoleName(user->role)) + "\",\"token\":\"" + token.substring(0, 8) + "...\"}";
    server.send(200, "application/json", response);
    
    Serial.println("[WEB] Response sent - Status: 200");
    Serial.println("[WEB] Cookie should be set now");
    Serial.println("[WEB] ========== LOGIN SUCCESS ==========\n");
  } else {
    Serial.println("[WEB] ✗ Authentication FAILED!");
    server.send(401, "application/json", "{\"success\":false,\"message\":\"Invalid username or password\"}");
    Serial.println("[WEB] ========== LOGIN FAILED ==========\n");
  }
}

void handleDashboard() {
  Serial.println("\n[WEB] ========== DASHBOARD REQUEST ==========");
  Serial.println("[WEB] Method: " + String(server.method() == HTTP_GET ? "GET" : "POST"));
  Serial.println("[WEB] URI: " + server.uri());
  
  // Debug all headers
  Serial.println("[WEB] Headers received:");
  for (int i = 0; i < server.headers(); i++) {
    Serial.println("  " + server.headerName(i) + ": " + server.header(i));
  }
  
  Session* session = getSessionFromRequest();
  
  if (session == nullptr) {
    Serial.println("[WEB] ✗ No valid session found");
    Serial.println("[WEB] Redirecting to login page");
    
    String redirectHtml = "<!DOCTYPE html><html><head>"
                         "<meta http-equiv=\"refresh\" content=\"0;url=/\">"
                         "</head><body>Redirecting to login...</body></html>";
    
    server.send(200, "text/html", redirectHtml);
    Serial.println("[WEB] ========================================\n");
    return;
  }
  
  Serial.println("[WEB] ✓ Valid session found!");
  Serial.println("[WEB] Username: " + session->username);
  Serial.println("[WEB] Role: " + String(getRoleName(session->role)));
  Serial.println("[WEB] Building dashboard HTML...");
  
  Permissions perms = getPermissions(session->role);
  String roleName = getRoleName(session->role);
  String roleClass = session->role == ADMIN ? "admin" : session->role == MODERATOR ? "moderator" : session->role == VIEWER ? "viewer" : "guest";
  
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>ESP32 LED Dashboard</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:Arial;background:#1a1a1a;color:white;padding:20px}.container{max-width:1200px;margin:0 auto}.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;padding:20px;background:#2a2a2a;border-radius:10px}.user-info{display:flex;align-items:center;gap:15px}.username{font-size:20px;font-weight:bold;color:#4CAF50}.role-badge{padding:6px 16px;border-radius:15px;font-size:13px;font-weight:bold}.role-badge.admin{background:#ff5722}.role-badge.moderator{background:#ff9800}.role-badge.viewer{background:#2196F3}.role-badge.guest{background:#9e9e9e}.logout-btn{padding:10px 20px;background:#f44336;color:white;border:none;border-radius:5px;cursor:pointer}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:20px}.card{background:#2a2a2a;border-radius:10px;padding:20px}.card h2{margin-bottom:15px;font-size:18px;color:#4CAF50}#ledPreview{width:120px;height:120px;margin:20px auto;border-radius:50%;border:4px solid #666;transition:all 0.3s;background:#333}.led-info{text-align:center;margin-top:15px;font-size:14px;color:#aaa}.led-info div{margin:5px 0}.button{padding:12px 25px;margin:5px;font-size:16px;cursor:pointer;border-radius:5px;border:none;transition:transform 0.1s;font-weight:500}.button:active{transform:scale(0.95)}.button:disabled{opacity:0.4;cursor:not-allowed}.on{background-color:#4CAF50;color:white}.off{background-color:#f44336;color:white}.color{background-color:#2196F3;color:white}.controls-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}#status{margin-top:15px;font-size:14px;padding:12px;background:#333;border-radius:5px;text-align:center}.access-denied{color:#ff9800;font-style:italic;text-align:center;padding:20px;background:rgba(255,152,0,0.1);border-radius:5px}.permissions-list{list-style:none;padding:0}.permissions-list li{padding:8px 0;border-bottom:1px solid #333;display:flex;align-items:center;gap:10px}.permissions-list li:last-child{border-bottom:none}#activityLog{max-height:300px;overflow-y:auto;font-size:13px}.log-entry{padding:8px;margin:5px 0;background:#1a1a1a;border-radius:5px;border-left:3px solid #4CAF50}.log-time{color:#888;font-size:11px}.log-user{color:#4CAF50;font-weight:bold}</style></head><body><div class=\"container\"><div class=\"header\"><div class=\"user-info\"><span class=\"username\">" + session->username + "</span><span class=\"role-badge " + roleClass + "\">" + roleName + "</span></div><button class=\"logout-btn\" onclick=\"logout()\">Logout</button></div><div class=\"grid\"><div class=\"card\"><h2>💡 LED Status</h2><div id=\"ledPreview\"></div><div class=\"led-info\"><div>State: <span id=\"ledStateText\">Loading...</span></div><div>RGB: <span id=\"ledRGB\">-</span></div></div></div><div class=\"card\"><h2>🔐 Permissions</h2><ul class=\"permissions-list\"><li>" + String(perms.canControlLED ? "✅" : "❌") + " Control LED</li><li>" + String(perms.canViewStatus ? "✅" : "❌") + " View Status</li><li>" + String(perms.canViewLogs ? "✅" : "❌") + " View Logs</li><li>" + String(perms.canChangeSettings ? "✅" : "❌") + " Settings</li><li>" + String(perms.canAccessAPI ? "✅" : "❌") + " API Access</li></ul></div></div><div class=\"card\" style=\"margin-bottom:20px\"><h2>🎮 LED Controls</h2>";
  
  if (perms.canControlLED) {
    html += "<div class=\"controls-grid\"><button class=\"button on\" onclick=\"ledControl('on')\">ON</button><button class=\"button off\" onclick=\"ledControl('off')\">OFF</button><button class=\"button color\" onclick=\"ledControl('red')\">Red</button><button class=\"button color\" onclick=\"ledControl('green')\">Green</button><button class=\"button color\" onclick=\"ledControl('blue')\">Blue</button><button class=\"button color\" onclick=\"ledControl('white')\">White</button><button class=\"button color\" onclick=\"ledControl('yellow')\">Yellow</button><button class=\"button color\" onclick=\"ledControl('cyan')\">Cyan</button><button class=\"button color\" onclick=\"ledControl('magenta')\">Magenta</button></div>";
  } else {
    html += "<div class=\"access-denied\">🔒 LED control requires Admin or Moderator privileges. Current role: " + roleName + "</div>";
  }
  
  html += "<div id=\"status\">Ready</div></div>";
  
  if (session->role == ADMIN) {
    html += "<div class=\"card\" style=\"margin-bottom:20px\"><h2>🔔 Buzzer Control (Admin Only)</h2><div class=\"controls-grid\"><button class=\"button on\" onclick=\"buzzerControl('on')\">Buzzer ON</button><button class=\"button off\" onclick=\"buzzerControl('off')\">Buzzer OFF</button><button class=\"button color\" onclick=\"buzzerControl('beep')\">🔊 Beep</button></div><div id=\"buzzerStatus\" style=\"margin-top:10px;font-size:14px;padding:10px;background:#333;border-radius:5px;text-align:center\">Ready</div></div>";
  }
  
  if (perms.canViewLogs) {
    html += "<div class=\"card\"><h2>📋 Activity Log</h2><div id=\"activityLog\">Loading...</div></div>";
  }
  
  html += "</div><script>const canControl=" + String(perms.canControlLED ? "true" : "false") + ";const canViewLogs=" + String(perms.canViewLogs ? "true" : "false") + ";const isAdmin=" + String(session->role == ADMIN ? "true" : "false") + ";function ledControl(c){if(!canControl){document.getElementById('status').innerHTML='🔒 Access Denied';return}document.getElementById('status').innerHTML='⏳ '+c;fetch('/led/'+c).then(r=>{if(r.status===403)throw new Error('Access Denied');if(!r.ok)throw new Error('HTTP '+r.status);return r.text()}).then(d=>{document.getElementById('status').innerHTML='✓ '+d;setTimeout(updateStatus,100)}).catch(e=>{document.getElementById('status').innerHTML='✗ '+e.message})}function buzzerControl(c){if(!isAdmin){document.getElementById('buzzerStatus').innerHTML='🔒 Access Denied';return}document.getElementById('buzzerStatus').innerHTML='⏳ Controlling buzzer...';fetch('/buzzer/'+c).then(r=>{if(r.status===403)throw new Error('Access Denied');if(!r.ok)throw new Error('HTTP '+r.status);return r.text()}).then(d=>{document.getElementById('buzzerStatus').innerHTML='✓ '+d}).catch(e=>{document.getElementById('buzzerStatus').innerHTML='✗ '+e.message})}function updateStatus(){fetch('/status').then(r=>r.json()).then(d=>{let p=document.getElementById('ledPreview');let s=document.getElementById('ledStateText');let rgb=document.getElementById('ledRGB');if(d.state==='on'){p.style.backgroundColor='rgb('+d.red+','+d.green+','+d.blue+')';p.style.boxShadow='0 0 40px rgba('+d.red+','+d.green+','+d.blue+',0.8)';s.textContent='ON';s.style.color='#4CAF50'}else{p.style.backgroundColor='#333';p.style.boxShadow='none';s.textContent='OFF';s.style.color='#f44336'}rgb.textContent='('+d.red+','+d.green+','+d.blue+')'}).catch(e=>console.error(e))}function updateLogs(){if(!canViewLogs)return;fetch('/logs').then(r=>r.json()).then(d=>{const logDiv=document.getElementById('activityLog');if(d.logs&&d.logs.length>0){logDiv.innerHTML=d.logs.map(log=>`<div class=\"log-entry\"><div class=\"log-time\">${log.timestamp}</div><div><span class=\"log-user\">${log.username}</span>: ${log.action}</div></div>`).join('')}else{logDiv.innerHTML='<div style=\"text-align:center;color:#888;padding:20px\">No activity</div>'}}).catch(e=>console.error(e))}function logout(){fetch('/logout').then(()=>window.location.href='/').catch(()=>window.location.href='/')}setInterval(updateStatus,1000);if(canViewLogs){setInterval(updateLogs,5000);updateLogs()}updateStatus();console.log('Dashboard loaded for user')</script></body></html>";
  
  server.send(200, "text/html", html);
  Serial.println("[WEB] Dashboard sent");
  Serial.println("[WEB] ========================================\n");
}

void handleLogs() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "application/json", "{\"error\":\"Not authenticated\"}");
    return;
  }
  
  Permissions perms = getPermissions(session->role);
  if (!perms.canViewLogs) {
    server.send(403, "application/json", "{\"error\":\"Insufficient privileges\"}");
    return;
  }
  
  String json = "{\"logs\":[";
  bool first = true;
  
  for (int i = 0; i < 50; i++) {
    int idx = (logIndex + i) % 50;
    if (activityLogs[idx].username.length() > 0) {
      if (!first) json += ",";
      json += "{\"timestamp\":\"" + activityLogs[idx].timestamp + "\",\"username\":\"" + activityLogs[idx].username + "\",\"action\":\"" + activityLogs[idx].action + "\"}";
      first = false;
    }
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleLogout() {
  Session* session = getSessionFromRequest();
  if (session != nullptr) {
    addLog(session->username, "Logged out");
    session->token = "";
  }
  server.sendHeader("Set-Cookie", "session=;Path=/;Max-Age=0");
  server.sendHeader("Location", "/");
  server.send(302, "text/html", "");
}

void handleLEDOn() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "text/plain", "Not authenticated");
    return;
  }
  
  Permissions perms = getPermissions(session->role);
  if (!perms.canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  
  if (ledState.red == 0 && ledState.green == 0 && ledState.blue == 0) {
    setLED(true, 255, 255, 255);
  } else {
    setLED(true, ledState.red, ledState.green, ledState.blue);
  }
  addLog(session->username, "LED turned ON");
  server.send(200, "text/plain", "LED turned ON");
}

void handleLEDOff() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "text/plain", "Not authenticated");
    return;
  }
  
  Permissions perms = getPermissions(session->role);
  if (!perms.canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  
  setLED(false, 0, 0, 0);
  addLog(session->username, "LED turned OFF");
  server.send(200, "text/plain", "LED turned OFF");
}

void handleRed() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 255, 0, 0);
  addLog(session->username, "LED set to RED");
  server.send(200, "text/plain", "LED set to RED");
}

void handleGreen() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 0, 255, 0);
  addLog(session->username, "LED set to GREEN");
  server.send(200, "text/plain", "LED set to GREEN");
}

void handleBlue() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 0, 0, 255);
  addLog(session->username, "LED set to BLUE");
  server.send(200, "text/plain", "LED set to BLUE");
}

void handleWhite() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 255, 255, 255);
  addLog(session->username, "LED set to WHITE");
  server.send(200, "text/plain", "LED set to WHITE");
}

void handleYellow() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 255, 255, 0);
  addLog(session->username, "LED set to YELLOW");
  server.send(200, "text/plain", "LED set to YELLOW");
}

void handleCyan() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 0, 255, 255);
  addLog(session->username, "LED set to CYAN");
  server.send(200, "text/plain", "LED set to CYAN");
}

void handleMagenta() {
  Session* session = getSessionFromRequest();
  if (session == nullptr || !getPermissions(session->role).canControlLED) {
    server.send(403, "text/plain", "Access Denied");
    return;
  }
  setLED(true, 255, 0, 255);
  addLog(session->username, "LED set to MAGENTA");
  server.send(200, "text/plain", "LED set to MAGENTA");
}

void handleBuzzerOn() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "text/plain", "Not authenticated");
    return;
  }
  
  if (session->role != ADMIN) {
    server.send(403, "text/plain", "Access Denied: Admin only");
    return;
  }
  
  digitalWrite(PIN_BUZZER, HIGH);
  addLog(session->username, "Buzzer turned ON");
  server.send(200, "text/plain", "Buzzer turned ON");
  Serial.println("[BUZZER] Turned ON by " + session->username);
}

void handleBuzzerOff() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "text/plain", "Not authenticated");
    return;
  }
  
  if (session->role != ADMIN) {
    server.send(403, "text/plain", "Access Denied: Admin only");
    return;
  }
  
  digitalWrite(PIN_BUZZER, LOW);
  addLog(session->username, "Buzzer turned OFF");
  server.send(200, "text/plain", "Buzzer turned OFF");
  Serial.println("[BUZZER] Turned OFF by " + session->username);
}

void handleBuzzerBeep() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "text/plain", "Not authenticated");
    return;
  }
  
  if (session->role != ADMIN) {
    server.send(403, "text/plain", "Access Denied: Admin only");
    return;
  }
  
  digitalWrite(PIN_BUZZER, HIGH);
  delay(200);
  digitalWrite(PIN_BUZZER, LOW);
  addLog(session->username, "Buzzer beeped");
  server.send(200, "text/plain", "Buzzer beeped");
  Serial.println("[BUZZER] Beeped by " + session->username);
}

void handleStatus() {
  Session* session = getSessionFromRequest();
  if (session == nullptr) {
    server.send(401, "application/json", "{\"error\":\"Not authenticated\"}");
    return;
  }
  
  String json = "{\"state\":\"" + String(ledState.isOn ? "on" : "off") + "\",\"red\":" + String(ledState.red) + ",\"green\":" + String(ledState.green) + ",\"blue\":" + String(ledState.blue) + ",\"brightness\":" + String(ledState.brightness) + "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  Serial.println("[WEB] 404: " + server.uri());
  server.send(404, "text/plain", "404: Not Found");
}

void setupWebServer() {
  Serial.println("\n========================================");
  Serial.println("Setting up Web Server with RBAC:");
  Serial.println("========================================");
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/dashboard", HTTP_GET, handleDashboard);
  server.on("/logs", HTTP_GET, handleLogs);
  
  server.on("/led/on", HTTP_GET, handleLEDOn);
  server.on("/led/off", HTTP_GET, handleLEDOff);
  server.on("/led/red", HTTP_GET, handleRed);
  server.on("/led/green", HTTP_GET, handleGreen);
  server.on("/led/blue", HTTP_GET, handleBlue);
  server.on("/led/white", HTTP_GET, handleWhite);
  server.on("/led/yellow", HTTP_GET, handleYellow);
  server.on("/led/cyan", HTTP_GET, handleCyan);
  server.on("/led/magenta", HTTP_GET, handleMagenta);
  
  server.on("/buzzer/on", HTTP_GET, handleBuzzerOn);
  server.on("/buzzer/off", HTTP_GET, handleBuzzerOff);
  server.on("/buzzer/beep", HTTP_GET, handleBuzzerBeep);
  
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("✓ Web Server started with RBAC");
  Serial.print("✓ Access at: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.println("\nTest Accounts:");
  Serial.println("  Admin: admin/admin123 - Full Control");
  Serial.println("  Mod: moderator/mod123 - LED Control");
  Serial.println("  Viewer: viewer/view123 - View Only");
  Serial.println("  Guest: guest/guest123 - Limited View");
  Serial.println("========================================\n");
  const char* headerKeys[] = {"Cookie"};
server.collectHeaders(headerKeys, 1);
}

void cleanExpiredSessions() {
  static unsigned long lastCleanup = 0;
  
  if (millis() - lastCleanup > 300000) {
    Serial.println("[SESSION] Cleaning expired sessions...");
    
    int cleaned = 0;
    for (int i = 0; i < 10; i++) {
      if (activeSessions[i].token.length() > 0) {
        if (millis() - activeSessions[i].loginTime > SESSION_TIMEOUT) {
          Serial.println("[SESSION] Removing expired session for: " + activeSessions[i].username);
          activeSessions[i].token = "";
          cleaned++;
        }
      }
    }
    
    Serial.println("[SESSION] Cleaned " + String(cleaned) + " expired sessions");
    lastCleanup = millis();
  }
}

bool controlLED(User user, bool turnOn) {
  if (user.role != ADMIN) {
    Serial.println("Access denied: Only admin can control LED.");
    return false;
  }
  digitalWrite(LED_PIN, turnOn ? HIGH : LOW);
  Serial.println(turnOn ? "LED turned ON" : "LED turned OFF");
  return true;
}

bool viewLEDStatus(User user) {
  int status = digitalRead(LED_PIN);
  Serial.print("LED status for ");
  Serial.print(user.username.c_str());
  Serial.print(": ");
  Serial.println(status == HIGH ? "ON" : "OFF");
  return status == HIGH;
}