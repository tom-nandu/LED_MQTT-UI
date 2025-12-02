┌─────────────────────────────────────────────────────────────────┐
│                     USER SPEAKS                                  │
│              "Turn on kitchen light"                             │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 1: Browser Microphone Capture                             │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  • Browser requests microphone permission (first time)          │
│  • Audio stream captured from device microphone                 │
│  • Raw audio buffered in memory                                 │
│  Duration: Continuous until voice detected                      │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 2: Web Speech API Processing (Browser/Cloud)              │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  • Voice Activity Detection (VAD) detects speech start          │
│  • Audio sent to Google Speech API (Chrome) or similar          │
│  • Speech-to-text conversion happens in cloud                   │
│  • Returns: "turn on kitchen light"                             │
│  Duration: 0.5-2 seconds                                         │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 3: JavaScript Event Fired                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  recognition.onresult = (event) => {                            │
│    const text = "turn on kitchen light"                         │
│    console.log("Heard:", text) // Logged                        │
│  }                                                               │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 4: Command Parsing & Pattern Matching                     │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  Input: "turn on kitchen light"                                 │
│  Process:                                                        │
│    1. Convert to lowercase: "turn on kitchen light"             │
│    2. Test against patterns:                                    │
│       ✗ /kitchen.*on/          → NO MATCH                       │
│       ✓ /turn on.*kitchen/     → MATCH!                         │
│    3. Map to command: "kitchen_light_on"                        │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 5: MQTT Message Creation                                  │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  publishMQTT("kitchen_light_on")                                │
│  {                                                               │
│    topic: "smarthome/control"                                   │
│    payload: "kitchen_light_on"                                  │
│    qos: 0                                                        │
│  }                                                               │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 6: WebSocket Transmission to MQTT Broker                  │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  Browser → Internet → broker.hivemq.com:8884 (WSS)              │
│  Encrypted WebSocket connection (TLS/SSL)                       │
│  Duration: 50-200ms (depends on internet speed)                 │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 7: MQTT Broker Message Routing                            │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  HiveMQ Broker receives message                                 │
│  • Checks topic: "smarthome/control"                            │
│  • Finds subscribers to this topic                              │
│  • Queues message for ESP32 client                              │
│  Duration: 10-50ms                                               │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 8: MQTT Message to ESP32                                  │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  HiveMQ → Internet → Your WiFi Router → ESP32                   │
│  TCP/IP connection (port 1883 or 8883)                          │
│  Duration: 50-200ms                                              │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 9: ESP32 MQTT Callback Triggered                          │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  void mqttCallback(char* topic, byte* payload, uint length) {   │
│    String message = "kitchen_light_on"                          │
│    handleLightControl(message)                                  │
│  }                                                               │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 10: ESP32 Command Parsing                                 │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  handleLightControl("kitchen_light_on") {                       │
│    command.trim()                                               │
│    command.toLowerCase() // "kitchen_light_on"                  │
│                                                                  │
│    if (command == "kitchen_light_on") {                         │
│      setLight(1, true) // Room 1, State ON                      │
│    }                                                             │
│  }                                                               │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 11: Relay Control                                         │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  void setLight(int room, bool state) {                          │
│    pin = RELAY1 (GPIO 10)                                       │
│    deviceState.kitchenLight = true                              │
│    digitalWrite(RELAY1, LOW) // Active LOW!                     │
│  }                                                               │
│  Duration: <1ms                                                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 12: Physical Relay Activation                             │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  • GPIO 10 goes LOW (0V)                                        │
│  • Relay coil energizes                                         │
│  • Relay contacts close                                         │
│  • 230V AC flows to kitchen light                               │
│  Duration: 10-20ms (mechanical relay)                           │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 13: Light Bulb Turns ON! 💡                               │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  Kitchen light illuminates                                      │
│  Duration: Instant (LED) or 100ms (CFL/Incandescent)            │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 14: Status Feedback (Parallel Process)                    │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  publishStatus() {                                              │
│    JSON: {                                                       │
│      "kitchen_light": "on",                                     │
│      "bed1_light": "off",                                       │
│      ...                                                         │
│    }                                                             │
│  }                                                               │
│  → Sent to MQTT topic: "smarthome/status"                       │
│  → Dashboard receives and updates UI                            │
│  Duration: 100-300ms round trip                                 │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  STEP 15: Dashboard UI Update                                   │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│  • Toggle switch turns green                                    │
│  • Light indicator turns on                                     │
│  • "1 ON" counter updates                                       │
│  • Activity log: "[14:32:15] ON Kitchen Light"                  │
│  Duration: <10ms (browser render)                               │
└─────────────────────────────────────────────────────────────────┘





  Browser (HTML Dashboard)
    ↓
  Voice Recognition (Web Speech API)
    ↓
  Parse Command → MQTT Message
    ↓
HiveMQ Broker (broker.hivemq.com)
    ↓
ESP32 (Subscribed to MQTT)
    ↓
  Control Relays/LED/Buzzer
    ↓
  Send Status Back → Dashboard
