/**
 * @file automated_thermometer_alarm_v3.ino
 * @brief ESP32-based MAX6675 temperature monitoring system with LittleFS web hosting,
 *        WebSocket communication, and a hardware Wi-Fi reset button.
 *
 * ## Overview
 * This firmware allows the ESP32 to:
 * - Host a real-time web dashboard via LittleFS (`index.html`, `stylesheet.css`, `script.js`)
 * - Connect to Wi-Fi using WiFiManager (no hardcoded credentials)
 * - Stream live MAX6675 thermocouple temperature readings over WebSocket
 * - Record, store, and transmit temperature logs to a connected browser client
 * - Send the device's IP and SSID to a Supabase database
 * - Reset Wi-Fi credentials using a momentary push button (hold 3s)
 *
 * ## Features
 * - WebSocket endpoint: `/ws`
 * - Static Wi-Fi setup portal SSID: `ESP32-Setup`
 * - Active-Low LED logic:
 *   - LOW = ON
 *   - HIGH = OFF
 * - Wi-Fi Reset Button (GPIO2): hold 3 seconds to clear credentials and reboot
 *
 * ## Author
 * @author
 * Mark Jayson Lanuzo
 * @date
 * 2025-11-05
 */

#include <WiFiManager.h>         ///< Simplified Wi-Fi setup & credential storage
#include <ESPAsyncWebServer.h>   ///< Asynchronous web server library
#include <AsyncTCP.h>            ///< Required by ESPAsyncWebServer
#include <ArduinoJson.h>         ///< JSON encoding/decoding
#include <max6675.h>             ///< MAX6675 thermocouple sensor library
#include <LittleFS.h>            ///< Filesystem for hosting dashboard files
#include <HTTPClient.h>          ///< HTTP client for Supabase REST calls
#include <vector>                ///< For dynamic temperature record storage

// -------------------- GLOBAL OBJECTS --------------------

AsyncWebServer server(80);       ///< HTTP server instance (port 80)
AsyncWebSocket ws("/ws");        ///< WebSocket endpoint for real-time communication

unsigned long lastSendTime = 0;  ///< Timestamp for temperature send interval
bool isRecording = false;        ///< Flag indicating if temperature logging is active
std::vector<float> recordedData; ///< Vector for storing recorded temperature values

AsyncWebSocketClient *activeClient = nullptr; ///< Pointer to currently active WebSocket client
bool clientConnected = false;                 ///< True if a client is connected to WebSocket

/** Supabase connection details */
const char* SUPABASE_URL = "https://ktzgfynmsnmmdvzgbjli.supabase.co";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imt0emdmeW5tc25tbWR2emdiamxpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjE4MDczNDIsImV4cCI6MjA3NzM4MzM0Mn0.4ElVmLBwxuvoo98pxtuWbNjIxJIm9KUVc7hIAXRCPv0"; // replace with your actual key

// -------------------- HARDWARE PINS --------------------

/** MAX6675 Thermocouple Pins */
const int thermoSO  = 19; ///< Serial Out (MISO)
const int thermoCS  = 5;  ///< Chip Select
const int thermoSCK = 18; ///< Serial Clock

MAX6675 thermocouple(thermoSCK, thermoCS, thermoSO); ///< Thermocouple sensor instance

/** LED Indicators (Active-Low) */
const int LED_GREEN  = 13; ///< Connection status LED
const int LED_BLUE   = 27; ///< Reading indicator LED
const int LED_RED    = 23; ///< Threshold alert LED
const int LED_YELLOW = 33; ///< Recording indicator LED

/** Wi-Fi Reset Button (Active-Low, GPIO2 → push button → GND) */
const int RESET_WIFI_BUTTON = 4;     
unsigned long buttonPressStart = 0;  ///< Track press duration for long-hold detection
bool buttonHeld = false;             ///< Track hold state
unsigned long lastButtonCheck = 0;   ///< Debounce timer for stable reads

/** LED Blink Timers */
bool redBlink = false;               ///< Red LED blinking flag
bool yellowBlink = false;            ///< Yellow LED blinking flag
unsigned long lastRedToggle = 0;     ///< Timestamp for red LED toggle
unsigned long lastYellowToggle = 0;  ///< Timestamp for yellow LED toggle
unsigned long lastBlueBlink = 0;     ///< Timestamp for blue LED pulse

// -------------------- FUNCTION DECLARATIONS --------------------

/**
 * @brief Initializes LED pins and ensures they are OFF (HIGH for active-low).
 */
void setupLEDs() {
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  // Active-low OFF state = HIGH
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_YELLOW, HIGH);
}

/**
 * @brief Sets LED state using active-low logic.
 * @param pin GPIO pin number.
 * @param state True = ON (LOW), False = OFF (HIGH).
 */
void setLED(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);
}

/**
 * @brief Non-blocking LED blink handler.
 * @param pin Target LED pin.
 * @param interval Time in ms between toggles.
 * @param lastToggle Reference to last toggle timestamp.
 */
void handleBlink(int pin, unsigned long interval, unsigned long &lastToggle) {
  unsigned long now = millis();
  if (now - lastToggle >= interval) {
    lastToggle = now;
    digitalWrite(pin, !digitalRead(pin)); // Toggle LED state
  }
}

/**
 * @brief Reads the current temperature from MAX6675 sensor.
 * @return Temperature in Celsius.
 */
float getTemperature() {
  return thermocouple.readCelsius();
}

/**
 * @brief Sends live temperature data to connected WebSocket client.
 *        Handles blue LED pulse, optional data logging, and JSON transmission.
 */
void sendTemperatureToClients() {
  float temp = getTemperature();

  // Briefly pulse blue LED once per second to indicate live reading
  if (millis() - lastBlueBlink >= 1000) {
    lastBlueBlink = millis();
    digitalWrite(LED_BLUE, LOW);
  }

  // Append data to record buffer if logging is active
  if (isRecording) recordedData.push_back(temp);

  // Build JSON payload: {"temperature": <value>}
  StaticJsonDocument<100> doc;
  doc["temperature"] = temp;
  String json;
  serializeJson(doc, json);

  // Send to active WebSocket client (if connected and ready)
  if (activeClient && activeClient->canSend()) {
    activeClient->text(json);
  }
}

/**
 * @brief Sends the current SSID and IP address to the Supabase database via REST API.
 */
void sendIP() {
  String ssid = WiFi.SSID(); // Current network SSID

  HTTPClient http;
  http.begin(String(SUPABASE_URL) + "/rest/v1/esp32_connections"); // Table REST endpoint
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer", "return=representation");

  String payload = "{\"ssid\":\"" + ssid + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";

  int httpResponseCode = http.POST(payload);
  Serial.printf("Supabase response: %d\n", httpResponseCode);
  http.end();
}

// -------------------- WIFI RESET BUTTON --------------------

/**
 * @brief Monitors the reset button during runtime.  
 *        If held ≥3s, clears all Wi-Fi credentials and restarts ESP32.
 * 
 * Implements debouncing and prevents repeated triggers.
 */
void checkResetWiFiButton() {
  // Debounce: read every 50 ms
  if (millis() - lastButtonCheck < 50) return;
  lastButtonCheck = millis();

  int buttonState = digitalRead(RESET_WIFI_BUTTON);

  if (buttonState == LOW) { // Button pressed (active low)
    if (!buttonHeld) {
      buttonHeld = true;
      buttonPressStart = millis();
      Serial.println("🔘 Button pressed...");
    }

    // Held for 3 seconds or more → trigger Wi-Fi reset
    if (millis() - buttonPressStart >= 3000) {
      Serial.println("🔄 Reset Wi-Fi button held — clearing credentials...");
      setLED(LED_YELLOW, true); // Indicate reset in progress

      WiFi.disconnect(true, true);  // Forget all stored Wi-Fi
      WiFiManager wm;
      wm.resetSettings();           // Clear WiFiManager config data

      delay(1000);
      Serial.println("♻️ Restarting ESP32...");
      ESP.restart();
      return;
    }
  } else {
    // Button released
    if (buttonHeld) {
      buttonHeld = false;
      Serial.println("Button released.");
    }
  }
}

// -------------------- WEBSOCKET HANDLERS --------------------

/**
 * @brief Parses and executes commands sent by the WebSocket client.
 * @param client Pointer to the WebSocket client instance.
 * @param msg Received message text.
 */
void handleClientCommand(AsyncWebSocketClient *client, const char *msg) {
  String message(msg);

  if (message == "test") {
    if (client != activeClient) {
      client->text("{\"error\":\"another user is already connected\"}");
      return;
    }
    client->text("{\"status\":\"ok\"}");

  } else if (message == "web_connected") {
    setLED(LED_GREEN, true);
  } else if (message == "web_disconnected") {
    setLED(LED_GREEN, false);
  } else if (message == "start_record") {
    isRecording = true;
    recordedData.clear();
    yellowBlink = true;
    client->text("{\"recording\":\"started\"}");
  } else if (message == "end_record") {
    isRecording = false;
    yellowBlink = false;
    setLED(LED_YELLOW, false);
    client->text("{\"recording\":\"stopped\"}");
  } else if (message == "threshold_alert_on") {
    redBlink = true;
  } else if (message == "threshold_alert_off") {
    redBlink = false;
    setLED(LED_RED, false);
  } else if (message == "get_record") {
    // Send all recorded data in JSON array format
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("data");
    for (float val : recordedData) arr.add(val);
    String json;
    serializeJson(doc, json);
    client->text(json);
  } else {
    client->text("{\"error\":\"unknown command\"}");
  }
}

/**
 * @brief WebSocket event handler for client connection, disconnection, and data reception.
 */
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // Allow only one active WebSocket client at a time
    if (clientConnected) {
      Serial.printf("⚠️ Rejecting extra client: %u\n", client->id());
      client->close();
      return;
    }
    activeClient = client;
    clientConnected = true;
    Serial.printf("✅ Client connected: %u\n", client->id());
    setLED(LED_GREEN, true);

  } else if (type == WS_EVT_DISCONNECT) {
    // Cleanup when client disconnects
    if (activeClient && client->id() == activeClient->id()) {
      activeClient = nullptr;
      clientConnected = false;
      Serial.println("❌ Client disconnected");
      setLED(LED_GREEN, false);
    }

  } else if (type == WS_EVT_DATA) {
    // Safely null-terminate message and process it
    ((char*)data)[len] = '\0';
    if (activeClient && client->id() == activeClient->id()) {
      handleClientCommand(client, (char*)data);
    }
  }
}

// -------------------- SETUP --------------------

/**
 * @brief Main setup routine — initializes serial port, LEDs, file system, Wi-Fi, and WebSocket server.
 */
void setup() {
  Serial.begin(115200);
  delay(500);

  setupLEDs();
  pinMode(RESET_WIFI_BUTTON, INPUT_PULLUP); // Enable internal pull-up for button

  delay(1000);

  // ---------- STARTUP RESET CHECK ----------
  if (digitalRead(RESET_WIFI_BUTTON) == LOW) {
    Serial.println("🔄 Reset button held during boot — clearing saved Wi-Fi...");
    setLED(LED_YELLOW, true);

    WiFi.disconnect(true, true);
    WiFiManager wm;
    wm.resetSettings();

    delay(1500);
    Serial.println("♻️ Restarting ESP32...");
    ESP.restart();
    return;
  }

  // ---------- FILESYSTEM ----------
  if (!LittleFS.begin()) {
    Serial.println("❌ LittleFS mount failed!");
    return;
  }

  // ---------- WIFI CONNECTION ----------
  WiFiManager wm;
  bool res = wm.autoConnect("ESP32-Setup", "12345678");
  if (!res) {
    Serial.println("❌ Wi-Fi connection failed");
    return;
  }

  Serial.println("✅ Connected to Wi-Fi");
  Serial.print("📡 IP: ");
  Serial.println(WiFi.localIP());

  // ---------- WEBSERVER ----------
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();

  // Report current IP and SSID to Supabase
  sendIP();

  Serial.print("🌐 Server started → ");
  Serial.println(WiFi.localIP());
}

// -------------------- LOOP --------------------

/**
 * @brief Main loop — manages WebSocket cleanup, LED blinking, Wi-Fi reset checks, and temperature streaming.
 */
void loop() {
  ws.cleanupClients();          // Remove disconnected WebSocket clients
  checkResetWiFiButton();       // Continuously check Wi-Fi reset button

  // Turn off blue LED after short blink pulse
  if (digitalRead(LED_BLUE) == LOW && millis() - lastBlueBlink >= 200)
    digitalWrite(LED_BLUE, HIGH);

  // Non-blocking blinking indicators
  if (redBlink) handleBlink(LED_RED, 500, lastRedToggle);
  if (yellowBlink) handleBlink(LED_YELLOW, 1000, lastYellowToggle);

  // Send temperature update every 1 second
  if (millis() - lastSendTime > 1000) {
    sendTemperatureToClients();
    lastSendTime = millis();
  }
}
