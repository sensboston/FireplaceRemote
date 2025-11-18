#include <IRremote.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ========================================
// CONFIGURATION CONSTANTS
// ========================================

// Pin definitions
#define IR_SEND_PIN 15
#define TEMP_SENSOR_PIN 4
#define BOOT_BUTTON_PIN 0  // Built-in BOOT button

// Button constants
const uint8_t BTN_POWER = 1;
const uint8_t BTN_MORE_HOT = 2;
const uint8_t BTN_LESS_HOT = 3;
const uint8_t BTN_TOGGLE_AIR = 4;

// Temperature filtering
const float TEMP_MIN_VALID = 30.0;      // Minimum valid temperature (°F)
const float TEMP_MAX_VALID = 120.0;     // Maximum valid temperature (°F)
const uint8_t TEMP_SAMPLES = 10;        // Number of samples for averaging
const float TEMP_UPDATE_THRESHOLD = 0.5; // Update UI only if change >= 0.5°F

// Temperature reading intervals
const unsigned long TEMP_READ_INTERVAL = 1000;     // Read sensor every 1 second
const unsigned long WEATHER_UPDATE_INTERVAL = 1800000; // Update weather every 30 min

// Thermostat defaults
const float DEFAULT_TARGET_TEMP = 72.0;           // Default target temperature (°F)
const float DEFAULT_TEMP_HYSTERESIS = 2.0;        // Turn on when temp drops 2°F below target
const float DEFAULT_OUTDOOR_TEMP_THRESHOLD = 50.0; // Only run when outdoor < 50°F
const uint8_t DEFAULT_START_HOUR = 8;             // Default schedule start: 8 AM
const uint8_t DEFAULT_END_HOUR = 22;              // Default schedule end: 10 PM
const uint8_t DEFAULT_DAYS_MASK = 0b01111111;     // All days enabled by default (Mon-Sun)

// Manual override duration (milliseconds)
const unsigned long MANUAL_OVERRIDE_DURATION = 300000; // 5 minutes

// Button colors for UI
const char* COLOR_POWER = "#e74c3c";        // Red
const char* COLOR_POWER_HOVER = "#c0392b";  // Dark red
const char* COLOR_MORE_HOT = "#e67e22";     // Orange
const char* COLOR_MORE_HOT_HOVER = "#d35400"; // Dark orange
const char* COLOR_LESS_HOT = "#27ae60";     // Green
const char* COLOR_LESS_HOT_HOVER = "#229954"; // Dark green
const char* COLOR_TOGGLE_AIR = "#3498db";   // Blue
const char* COLOR_TOGGLE_AIR_HOVER = "#2980b9"; // Dark blue

// OpenWeatherMap API
#include "secrets.h"
const String WEATHER_API_URL = "http://api.openweathermap.org/data/2.5/weather?q=" + 
                               WEATHER_CITY + "," + WEATHER_COUNTRY + 
                               "&appid=" + WEATHER_API_KEY + "&units=imperial";

// NTP Configuration for EST/EDT
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -5 * 3600;  // EST is UTC-5
const int DAYLIGHT_OFFSET_SEC = 3600;   // DST adds 1 hour

// Preferences namespace
const char* PREFS_NAMESPACE = "fireplace";

// ========================================
// IR COMMAND RAW DATA
// ========================================

// Power button - pause ~7150us between repeats
uint16_t rawPower[] = {
    1250, 450,
    1250, 400, 400, 1250, 1250, 400, 1250, 400,
    400, 1300, 400, 1300, 400, 1250, 400, 1300,
    400, 1300, 400, 1250, 1250, 7150
};

// More hot button - pause ~8000us between repeats
uint16_t rawMoreHot[] = {
    1250, 450,
    1250, 450, 400, 1300, 1250, 450, 1250, 450,
    400, 1300, 400, 1300, 1250, 450, 400, 1300,
    400, 1300, 400, 1250, 400, 8000
};

// Less hot button - pause ~8000us between repeats
uint16_t rawLessHot[] = {
    1250, 450,
    1250, 450, 400, 1250, 1250, 450, 1250, 450,
    350, 1300, 400, 1300, 400, 1300, 1250, 450,
    400, 1300, 400, 1250, 400, 8000
};

// Toggle air button - pause ~8000us between repeats
uint16_t rawToggleAir[] = {
    1250, 400,
    1300, 400, 400, 1300, 1250, 400, 1250, 450,
    400, 1250, 400, 1300, 400, 1250, 400, 1300,
    1250, 400, 400, 1300, 400, 8000
};

// ========================================
// GLOBAL VARIABLES
// ========================================

// Temperature sensor
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature sensors(&oneWire);

// Web server
AsyncWebServer server(80);

// Preferences storage
Preferences preferences;

// Temperature tracking
float tempReadings[TEMP_SAMPLES];
uint8_t tempIndex = 0;
float currentTempF = 0.0;
float lastDisplayedTempF = 0.0;
unsigned long lastTempRead = 0;

// Weather tracking
float outdoorTempF = 999.0; // 999 = unknown
unsigned long lastWeatherUpdate = 0;

// Thermostat state
bool thermostatEnabled = false;
float targetTemp = DEFAULT_TARGET_TEMP;
uint8_t daysOfWeek = DEFAULT_DAYS_MASK; // Bit 0=Mon, 1=Tue, ..., 6=Sun
uint8_t startHour = DEFAULT_START_HOUR;
uint8_t endHour = DEFAULT_END_HOUR;
float tempHysteresis = DEFAULT_TEMP_HYSTERESIS;
float outdoorTempThreshold = DEFAULT_OUTDOOR_TEMP_THRESHOLD;
bool fireplaceOn = false; // Internal state tracking

// Manual override tracking
unsigned long manualOverrideUntil = 0; // Timestamp when manual override expires

// Time tracking
struct tm timeinfo;

// WiFi reconnection tracking
unsigned long wifiDisconnectedTime = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 30000; // Try to reconnect every 30 seconds
bool wifiReconnecting = false;

// ========================================
// FUNCTION PROTOTYPES
// ========================================
void readTemperature();
float getFilteredTemperature();
void updateOutdoorWeather();
void sendIRCommand(uint8_t button);
void thermostatLogic();
bool isInSchedule();
void loadSettings();
void saveSettings();
void saveFireplaceState();
void WiFiEvent(WiFiEvent_t event);
void handleWiFiReconnection();

// ========================================
// HTML PAGE WITH PWA SUPPORT
// ========================================

// ========================================
// HTML PAGE SELECTION
// ========================================
// Comment out USE_COMPRESSED_HTML to use readable version (html.h) for development/debugging
// Uncomment for production to save ~7KB Flash memory

#define USE_COMPRESSED_HTML

#ifdef USE_COMPRESSED_HTML
  #include "html_compressed.h"  // Minified version (22.9 KB) - Production
#else
  #include "html.h"             // Readable version (30.1 KB) - Development
#endif


// ========================================
// TEMPERATURE FUNCTIONS
// ========================================

void readTemperature()
{
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    
    if (tempC != DEVICE_DISCONNECTED_C) {
        float tempF = (tempC * 9.0 / 5.0) + 32.0;
        
        // Filter out invalid readings
        if (tempF >= TEMP_MIN_VALID && tempF <= TEMP_MAX_VALID) {
            tempReadings[tempIndex] = tempF;
            tempIndex = (tempIndex + 1) % TEMP_SAMPLES;
            
            currentTempF = getFilteredTemperature();
        }
    } else {
        Serial.println("Error: Temperature sensor disconnected!");
    }
}

float getFilteredTemperature()
{
    // Calculate average of valid readings
    float sum = 0;
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < TEMP_SAMPLES; i++) {
        if (tempReadings[i] >= TEMP_MIN_VALID && tempReadings[i] <= TEMP_MAX_VALID) {
            sum += tempReadings[i];
            count++;
        }
    }
    
    return (count > 0) ? (sum / count) : 0.0;
}

// ========================================
// WEATHER FUNCTIONS
// ========================================

void updateOutdoorWeather()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, skipping weather update");
        return;
    }
    
    HTTPClient http;
    http.begin(WEATHER_API_URL);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String payload = http.getString();
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            outdoorTempF = doc["main"]["temp"].as<float>();
            Serial.print("Outdoor temperature: ");
            Serial.print(outdoorTempF, 1);
            Serial.println(" °F");
        } else {
            Serial.println("Failed to parse weather data");
        }
    } else {
        Serial.print("Weather API error: ");
        Serial.println(httpCode);
    }
    
    http.end();
}

// ========================================
// IR COMMAND FUNCTION
// ========================================

void sendIRCommand(uint8_t button)
{
    uint16_t* rawData;
    uint16_t length;
    const char* buttonName;

    switch(button)
    {
        case BTN_POWER:
            rawData = rawPower;
            length = sizeof(rawPower) / sizeof(rawPower[0]);
            buttonName = "Power";
            break;

        case BTN_MORE_HOT:
            rawData = rawMoreHot;
            length = sizeof(rawMoreHot) / sizeof(rawMoreHot[0]);
            buttonName = "More Hot";
            break;

        case BTN_LESS_HOT:
            rawData = rawLessHot;
            length = sizeof(rawLessHot) / sizeof(rawLessHot[0]);
            buttonName = "Less Hot";
            break;

        case BTN_TOGGLE_AIR:
            rawData = rawToggleAir;
            length = sizeof(rawToggleAir) / sizeof(rawToggleAir[0]);
            buttonName = "Toggle Air";
            break;

        default:
            Serial.println("Unknown button");
            return;
    }

    Serial.print("IR: ");
    Serial.println(buttonName);

    IrSender.sendRaw(rawData, length, 38);

    Serial.println("IR: Done");
}

// ========================================
// THERMOSTAT LOGIC
// ========================================

bool isInSchedule()
{
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    
    // Check day of week (tm_wday: 0=Sun, 1=Mon, ..., 6=Sat)
    // Convert to our format (0=Mon, 1=Tue, ..., 6=Sun)
    uint8_t dayBit = (timeinfo.tm_wday == 0) ? 6 : (timeinfo.tm_wday - 1);
    
    if (!((daysOfWeek >> dayBit) & 1)) {
        return false; // Day not enabled
    }
    
    // Check hour
    uint8_t currentHour = timeinfo.tm_hour;
    
    if (startHour <= endHour) {
        return (currentHour >= startHour && currentHour < endHour);
    } else {
        // Crosses midnight
        return (currentHour >= startHour || currentHour < endHour);
    }
}

void thermostatLogic()
{
    if (!thermostatEnabled) {
        return;
    }

    // Check if manual override is active
    unsigned long currentTime = millis();
    if (manualOverrideUntil > 0 && currentTime < manualOverrideUntil) {
        return;
    } else if (manualOverrideUntil > 0) {
        Serial.println("Manual override expired - auto control resumed");
        manualOverrideUntil = 0;
    }

    // Check outdoor temperature threshold
    if (outdoorTempF != 999.0 && outdoorTempF >= outdoorTempThreshold) {
        if (fireplaceOn) {
            sendIRCommand(BTN_POWER);
            fireplaceOn = false;
            saveFireplaceState();
            Serial.print("Fireplace OFF: outdoor ");
            Serial.print(outdoorTempF, 1);
            Serial.print("°F >= ");
            Serial.println(outdoorTempThreshold, 1);
        }
        return;
    }

    // Check schedule
    if (!isInSchedule()) {
        if (fireplaceOn) {
            sendIRCommand(BTN_POWER);
            fireplaceOn = false;
            saveFireplaceState();
            Serial.println("Fireplace OFF: outside schedule");
        }
        return;
    }

    // Temperature control logic
    float turnOnTemp = targetTemp - tempHysteresis;

    if (!fireplaceOn && currentTempF < turnOnTemp) {
        Serial.print(">>> ON: ");
        Serial.print(currentTempF, 1);
        Serial.print(" < ");
        Serial.println(turnOnTemp, 1);
        sendIRCommand(BTN_POWER);
        fireplaceOn = true;
        saveFireplaceState();
    }
    else if (fireplaceOn && currentTempF >= targetTemp) {
        Serial.print(">>> OFF: ");
        Serial.print(currentTempF, 1);
        Serial.print(" >= ");
        Serial.println(targetTemp, 1);
        sendIRCommand(BTN_POWER);
        fireplaceOn = false;
        saveFireplaceState();
    }
}

// ========================================
// SETTINGS FUNCTIONS
// ========================================

void loadSettings()
{
    preferences.begin(PREFS_NAMESPACE, false);

    thermostatEnabled = preferences.getBool("enabled", false);
    targetTemp = preferences.getFloat("target", DEFAULT_TARGET_TEMP);
    daysOfWeek = preferences.getUChar("days", DEFAULT_DAYS_MASK);
    startHour = preferences.getUChar("startHour", DEFAULT_START_HOUR);
    endHour = preferences.getUChar("endHour", DEFAULT_END_HOUR);
    tempHysteresis = preferences.getFloat("hysteresis", DEFAULT_TEMP_HYSTERESIS);
    outdoorTempThreshold = preferences.getFloat("threshold", DEFAULT_OUTDOOR_TEMP_THRESHOLD);
    fireplaceOn = preferences.getBool("fireplaceOn", false);

    preferences.end();

    Serial.println("Settings loaded");
    Serial.print("Thermostat: ");
    Serial.println(thermostatEnabled ? "ON" : "OFF");
    Serial.print("Target: ");
    Serial.print(targetTemp, 1);
    Serial.println("°F");
    Serial.print("Fireplace: ");
    Serial.println(fireplaceOn ? "ON" : "OFF");
}

void saveSettings()
{
    preferences.begin(PREFS_NAMESPACE, false);

    preferences.putBool("enabled", thermostatEnabled);
    preferences.putFloat("target", targetTemp);
    preferences.putUChar("days", daysOfWeek);
    preferences.putUChar("startHour", startHour);
    preferences.putUChar("endHour", endHour);
    preferences.putFloat("hysteresis", tempHysteresis);
    preferences.putFloat("threshold", outdoorTempThreshold);
    preferences.putBool("fireplaceOn", fireplaceOn);

    preferences.end();

    Serial.println("Settings saved");
}

void saveFireplaceState()
{
    preferences.begin(PREFS_NAMESPACE, false);
    preferences.putBool("fireplaceOn", fireplaceOn);
    preferences.end();
}

// ========================================
// WIFI RECONNECTION FUNCTIONS
// ========================================

void WiFiEvent(WiFiEvent_t event)
{
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("WiFi connected! IP: ");
            Serial.println(WiFi.localIP());
            wifiReconnecting = false;
            wifiDisconnectedTime = 0;

            // Resync time after reconnection
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

            // Update weather after reconnection
            updateOutdoorWeather();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi connection lost!");
            if (!wifiReconnecting) {
                wifiDisconnectedTime = millis();
                wifiReconnecting = true;
                Serial.println("Will attempt to reconnect...");
            }
            break;

        default:
            break;
    }
}

void handleWiFiReconnection()
{
    if (wifiReconnecting && WiFi.status() != WL_CONNECTED) {
        unsigned long currentTime = millis();

        // Check if enough time has passed since last reconnection attempt
        if (currentTime - wifiDisconnectedTime >= WIFI_RECONNECT_INTERVAL) {
            Serial.println("Attempting WiFi reconnection...");
            WiFi.reconnect();
            wifiDisconnectedTime = currentTime; // Reset timer for next attempt
        }
    }
}

// ========================================
// SETUP
// ========================================

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== Fireplace Remote Control ===");

    // Check if BOOT button is pressed for factory reset
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    delay(100);

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        Serial.println("\n*** FACTORY RESET ***");
        
        preferences.begin(PREFS_NAMESPACE, false);
        preferences.clear();
        preferences.end();
        
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        
        Serial.println("Reset complete - restarting...");
        delay(2000);
        ESP.restart();
    }

    // Initialize
    for (uint8_t i = 0; i < TEMP_SAMPLES; i++) {
        tempReadings[i] = 0;
    }
    
    sensors.begin();
    Serial.println("Sensor initialized");
    
    loadSettings();

    // Register WiFi event handler for automatic reconnection
    WiFi.onEvent(WiFiEvent);

    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);

    if (!wifiManager.autoConnect("FireplaceRemote")) {
        Serial.println("WiFi timeout - restarting");
        delay(3000);
        ESP.restart();
    }

    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
        delay(1000);
        retries++;
    }
    
    IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK);
    Serial.println("IR ready");
    
    updateOutdoorWeather();
    readTemperature();
    
    // Web server routes

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        // Get current time
        time_t now;
        time(&now);

        // Create initial data script to inject into HTML
        String initialDataScript = "<script>const INITIAL_DATA = {"
                                   "\"indoor\":" + String(currentTempF, 1) +
                                   ",\"outdoor\":" + String(outdoorTempF, 1) +
                                   ",\"fireplaceOn\":" + String(fireplaceOn ? "true" : "false") +
                                   ",\"timestamp\":" + String(now) +
                                   "};</script>";

        // Combine static HTML with injected data
        String html = String(index_html);
        html.replace("</head>", initialDataScript + "</head>");

        request->send(200, "text/html", html);
    });

    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
        time_t now;
        time(&now);

        String json = "{\"indoor\":" + String(currentTempF, 1) +
                      ",\"outdoor\":" + String(outdoorTempF, 1) +
                      ",\"fireplaceOn\":" + String(fireplaceOn ? "true" : "false") +
                      ",\"timestamp\":" + String(now) + "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"enabled\":" + String(thermostatEnabled ? "true" : "false") +
                      ",\"target\":" + String((int)targetTemp) +
                      ",\"days\":" + String(daysOfWeek) +
                      ",\"startHour\":" + String(startHour) +
                      ",\"endHour\":" + String(endHour) +
                      ",\"hysteresis\":" + String(tempHysteresis, 1) +
                      ",\"threshold\":" + String(outdoorTempThreshold, 1) + "}";
        request->send(200, "application/json", json);
    });
    
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            thermostatEnabled = doc["enabled"];
            targetTemp = doc["target"];
            daysOfWeek = doc["days"];
            startHour = doc["startHour"];
            endHour = doc["endHour"];
            
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid JSON");
        }
    });
    
    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("button")) {
            uint8_t button = request->getParam("button")->value().toInt();

            if (button >= BTN_POWER && button <= BTN_TOGGLE_AIR) {
                sendIRCommand(button);

                if (button == BTN_POWER) {
                    fireplaceOn = !fireplaceOn;
                    saveFireplaceState();
                }

                manualOverrideUntil = millis() + MANUAL_OVERRIDE_DURATION;
                Serial.println("Manual override: 5 min");

                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid button");
            }
        } else {
            request->send(400, "text/plain", "Missing button");
        }
    });

    server.on("/advanced", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, data, len);

        if (!error) {
            tempHysteresis = doc["hysteresis"];
            outdoorTempThreshold = doc["threshold"];
            saveSettings();
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Invalid JSON");
        }
    });

    server.on("/resetwifi", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Resetting...");
        delay(1000);
        
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        delay(500);
        ESP.restart();
    });

    server.begin();
    Serial.println("=== Server started ===");
    Serial.print("URL: http://");
    Serial.println(WiFi.localIP());
}

// ========================================
// LOOP
// ========================================

void loop()
{
    unsigned long currentMillis = millis();

    // Handle WiFi reconnection if needed
    handleWiFiReconnection();

    if (currentMillis - lastTempRead >= TEMP_READ_INTERVAL) {
        lastTempRead = currentMillis;
        readTemperature();
        thermostatLogic();
    }

    if (currentMillis - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
        lastWeatherUpdate = currentMillis;
        updateOutdoorWeather();
    }
}
