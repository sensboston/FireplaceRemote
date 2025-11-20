# Fireplace Remote Control


![Screenshot_20251118_222841_Easy PWA](https://github.com/user-attachments/assets/17a3857c-e729-4103-a444-b63d7a4d3bb8)
![Screenshot_20251119_162406_Easy PWA](https://github.com/user-attachments/assets/8807ea2a-9acb-47fb-9802-e7618a5c2446)

_Web app screenshots_

![20251118_182410](https://github.com/user-attachments/assets/0a6497ae-8979-4a4e-8e28-708752120b4c)

_Prototype_

![20251119_163200](https://github.com/user-attachments/assets/5cee2718-a5c9-4454-b4c4-04014ea85ab9)

_Production device_


## WiFi-enabled remote control system for Duraflame electric fireplaces using ESP32 microcontroller.


## Features

- **WiFi Control**: Web-based Progressive Web App (PWA) for mobile devices
  - Instant data loading on connection (no initial HTTP requests)
  - Real-time temperature and time display
- **WiFi Resilience**: Automatic reconnection after network outages or power loss
  - Event-driven reconnection monitoring
  - Auto-resync time and weather after reconnection
- **IR Transmission**: Controls Duraflame electric fireplace via infrared signals
- **Temperature Monitoring**: Real-time indoor temperature sensing with Dallas DS18B20 sensor
- **Smart Thermostat**: Automatic temperature control with scheduling
  - Configurable target temperature
  - Temperature hysteresis control
  - Day-of-week scheduling (Mon-Sun)
  - Time-based scheduling (start/end hours)
  - Manual override (5 minutes)
- **Weather Integration**: Outdoor temperature from OpenWeatherMap API
- **Temperature Units**: Toggle between Fahrenheit and Celsius
- **Advanced Settings**:
  - Configurable temperature hysteresis
  - Outdoor temperature threshold
  - WiFi reset functionality
- **Factory Reset**: Hardware reset via built-in BOOT button
- **Mobile-Optimized UI**: No hover effects, touch-friendly buttons

## Hardware Requirements

- **ESP32 Development Board** (tested with ESP32-WROOM-32) [Amazon](https://www.amazon.com/ESP-WROOM-32-Development-Microcontroller-Integrated-Compatible/dp/B08D5ZD528)
- **IR LED Transmitter** (38kHz) [Amazon](https://www.amazon.com/Digital-Receiver-Transmitter-Electronic-Building/dp/B0DSVZ7NNC)
- **Dallas DS18B20 Temperature Sensor** (1-Wire) [Amazon](https://www.amazon.com/dp/B0CPXKS54H)
- **4.7kΩ Resistor** (pull-up for DS18B20 data line)
- **Current Limiting Resistor** for IR LED (typically 100-330Ω)
- **Duraflame Electric Fireplace** (compatible model)

## Wiring Diagram

### Pin Connections

| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| IR LED (+) | GPIO 15 | Via current limiting resistor |
| IR LED (-) | GND | |
| DS18B20 VCC | 3.3V | |
| DS18B20 GND | GND | |
| DS18B20 Data | GPIO 4 | 4.7kΩ pull-up to 3.3V |
| BOOT Button | GPIO 0 | Built-in (for factory reset) |

### DS18B20 Wiring

```
ESP32 3.3V ----+---- DS18B20 VCC
               |
              4.7kΩ
               |
ESP32 GPIO 4 --+---- DS18B20 Data

ESP32 GND  --------- DS18B20 GND
```

### IR LED Wiring

```
ESP32 GPIO 15 ---- [100-330Ω] ---- IR LED (+)
ESP32 GND     -------------------- IR LED (-)
```

## Software Dependencies

### Arduino Libraries

Install via Arduino Library Manager or PlatformIO:

- `WiFiManager` by tzapu (>=2.0.0)
- `ESPAsyncWebServer` by me-no-dev
- `AsyncTCP` by me-no-dev
- `IRremote` by Arduino-IRremote
- `OneWire` by Paul Stoffregen
- `DallasTemperature` by Miles Burton
- `ArduinoJson` by Benoit Blanchon (>=6.0.0)

## Installation

1. **Clone Repository**
   ```bash
   git clone https://github.com/sensboston/FireplaceRemote.git
   cd FireplaceRemote
   ```

2. **Configure OpenWeatherMap API** (Optional but recommended)
   - Get free API key from [OpenWeatherMap](https://openweathermap.org/api)
   - Copy the sample secrets file:
     ```bash
     cp secrets.sample secrets.h
     ```
   - Edit `secrets.h` with your credentials:
     ```cpp
     const String WEATHER_API_KEY = "your_api_key_here";
     const String WEATHER_CITY = "YourCity";
     const String WEATHER_COUNTRY = "US";
     ```
   - **Important**: `secrets.h` is already in `.gitignore` and won't be committed

3. **Open in Arduino IDE**
   - Open `FireplaceRemote.ino`
   - Select Board: `ESP32 Dev Module`
   - Select Port: Your ESP32 COM port

4. **Upload to ESP32**
   - Click Upload button
   - Wait for compilation and upload

5. **Initial WiFi Setup**
   - ESP32 will create WiFi access point: `FireplaceRemote`
   - Connect to this AP with your phone/computer
   - Browser will open configuration page automatically
   - Select your WiFi network and enter password
   - ESP32 will restart and connect to your WiFi

## Usage

### Web Interface

1. **Connect to Web App**
   - Find ESP32 IP address in Serial Monitor, or
   - Check your router's DHCP client list
   - Open browser: `http://[ESP32_IP_ADDRESS]`
   - Add to Home Screen for PWA experience

2. **Control Buttons**
   - **POWER ON/OFF** (Red) - Toggle fireplace power
   - **HOT +** (Orange) - Increase heat level
   - **HOT -** (Green) - Decrease heat level
   - **AIR FLOW TOGGLE** (Blue) - Toggle air circulation

3. **Thermostat Settings**
   - Enable "Keep Temperature" checkbox
   - Select target temperature (60-80°F or 15-27°C)
   - Configure schedule:
     - Select active days (Mon-Sun)
     - Set start and end hours
   - Settings auto-save on change

4. **Advanced Settings**
   - Click **+** button to expand
   - **Temperature Hysteresis**: How many degrees below target to turn on (0.5-10°F)
   - **Outdoor Temperature Threshold**: Don't run fireplace when outdoor temp exceeds this (30-80°F)
   - **Reset WiFi Settings**: Factory reset WiFi configuration

### Factory Reset

To perform complete factory reset:

1. Disconnect power from ESP32
2. Press and hold **BOOT** button (GPIO 0)
3. Connect power while holding BOOT
4. Wait 2 seconds, then release
5. ESP32 will:
   - Clear all stored settings
   - Reset WiFi configuration
   - Restart in AP mode

### API Endpoints

The web server exposes these REST endpoints:

- `GET /` - Main web interface (HTML)
- `GET /temperature` - Get indoor/outdoor temperatures (JSON)
- `GET /settings` - Get thermostat settings (JSON)
- `POST /settings` - Save thermostat settings (JSON body)
- `POST /advanced` - Save advanced settings (JSON body)
- `GET /cmd?button=[1-4]` - Send IR command (1=Power, 2=Hot+, 3=Hot-, 4=Air)
- `POST /resetwifi` - Reset WiFi settings and restart

## Technical Details

### WiFi Reconnection

The system uses ESP32 WiFi Events for robust connection management:

- **Event-Driven**: `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` triggers automatic reconnection
- **Retry Interval**: Attempts reconnection every 30 seconds during outages
- **Post-Reconnect**: Automatically resyncs NTP time and updates weather data
- **Handles**: Power outages, router restarts, network blackouts

### Instant Data Loading

When clients connect to the web interface:

- **Server-Side Injection**: Temperature, time, and fireplace state embedded directly in HTML
- **No Initial Delay**: Data displays immediately without waiting for HTTP requests
- **Update Cycle**: Continues polling every 5 seconds for real-time updates

### HTML Optimization

The web interface uses a dual-file system for optimal development and production:

- **Development**: `html.h` - Readable, well-formatted HTML/CSS/JS with comments (30.1 KB)
- **Production**: `html_compressed.h` - Minified version without comments/whitespace (22.9 KB)
- **Savings**: 7.2 KB (24% reduction) in Flash memory
- **Selection**: Controlled by `USE_COMPRESSED_HTML` define in `FireplaceRemote.ino`

**For Development:**
```cpp
// #define USE_COMPRESSED_HTML  // Comment out
```

**For Production:**
```cpp
#define USE_COMPRESSED_HTML  // Uncomment (default)
```

Editing: Always edit `html.h` for UI changes, then regenerate `html_compressed.h`

### Temperature Control Logic

The thermostat uses hysteresis to prevent rapid on/off cycling:

- **Turn ON**: When indoor temp drops below `(target - hysteresis)`
- **Turn OFF**: When indoor temp reaches `target`
- **Outdoor Check**: Fireplace disabled when outdoor temp ≥ threshold
- **Schedule Check**: Only operates during configured days/hours
- **Manual Override**: 5-minute pause on manual button press

### Temperature Filtering

Indoor temperature uses 10-sample moving average filter to reduce noise.

### IR Signal Details

- **Protocol**: Raw IR (38kHz carrier)

### Data Persistence

Settings stored in ESP32 NVS (Non-Volatile Storage):
- Thermostat enabled/disabled
- Target temperature
- Schedule (days, start/end hours)
- Temperature hysteresis
- Outdoor temperature threshold
- Fireplace on/off state

## Troubleshooting

### Buttons Don't Work
- Check IR LED wiring and polarity
- Verify GPIO 15 connection
- Ensure proper current limiting resistor
- Check serial monitor for "IR: Done" messages

### Temperature Reads Error
- Verify DS18B20 wiring
- Check 4.7kΩ pull-up resistor
- Test sensor with separate sketch
- Check for "Temperature sensor disconnected" in serial monitor

### WiFi Won't Connect
- Use factory reset (BOOT button method)
- Verify WiFi credentials
- Check 2.4GHz network (ESP32 doesn't support 5GHz)
- Ensure router allows new device connections

### WiFi Keeps Disconnecting
- Check Serial Monitor for "WiFi connection lost!" messages
- System will automatically attempt reconnection every 30 seconds
- If reconnection fails repeatedly:
  - Check WiFi signal strength (ESP32 should be within router range)
  - Verify router stability
  - Check for MAC address filtering on router
  - Consider reducing WiFi sleep mode if enabled

### Web Page Won't Load
- Check ESP32 IP address in serial monitor
- Verify device on same WiFi network
- Try accessing via IP instead of hostname
- Clear browser cache

## Configuration Constants

Key constants that can be modified in `FireplaceRemote.ino`:

```cpp
// Temperature thresholds
const float DEFAULT_TARGET_TEMP = 72.0;           // Default target (°F)
const float DEFAULT_TEMP_HYSTERESIS = 2.0;        // Default hysteresis (°F)
const float DEFAULT_OUTDOOR_TEMP_THRESHOLD = 50.0; // Outdoor threshold (°F)

// Schedule defaults
const uint8_t DEFAULT_START_HOUR = 8;             // 8 AM
const uint8_t DEFAULT_END_HOUR = 22;              // 10 PM
const uint8_t DEFAULT_DAYS_MASK = 0b01111111;     // All days enabled

// Timing
const unsigned long TEMP_READ_INTERVAL = 1000;     // 1 second
const unsigned long WEATHER_UPDATE_INTERVAL = 1800000; // 30 minutes
const unsigned long MANUAL_OVERRIDE_DURATION = 300000; // 5 minutes
const unsigned long WIFI_RECONNECT_INTERVAL = 30000; // 30 seconds
```

## License

This project is open source. Feel free to modify and distribute.

## Credits

- **Author**: [sensboston](https://github.com/sensboston)
- **Hardware**: ESP32, Duraflame Electric Fireplace
- **Libraries**: WiFiManager, ESPAsyncWebServer, IRremote, DallasTemperature

## Support

For issues, questions, or contributions, please open an issue on GitHub.
