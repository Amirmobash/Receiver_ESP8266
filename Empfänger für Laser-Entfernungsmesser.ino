// ============================================================================
//  Receiver_ESP826 Receiver – ESP8266 + OLED
//  Board:   NodeMCU 1.0 (ESP-12E)
//  Display: 1.3" I2C OLED (SSD1306 compatible)
//
//  Description:
//    This project connects an ESP8266 NodeMCU to a laser measurement sender
//    (ESP8266 Access Point with HTTP JSON) and displays the distance in meters
//    on a 1.3" I2C OLED display.
//
//  Author:  Amir Mobasheraghdam
//  GitHub:  https://github.com/Amirmobash
//  Website: https://www.nivta.de
//
//  File:    Receiver_ESP826_Receiver_ESP8266_Amir_Mobasheraghdam.ino
// ============================================================================

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------------------------
// OLED configuration
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// I2C pins for NodeMCU 1.0 (ESP-12E)
//   SDA -> D2 (GPIO4)
//   SCL -> D1 (GPIO5)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------------------------------------------------------------------
// WiFi / Laser sender configuration
// ---------------------------------------------------------------------------
// These values must match the Access Point created by the laser ESP8266 board.
const char* WIFI_SSID   = "LaserESP8266"; // Access Point SSID from laser board
const char* WIFI_PASS   = "";             // leave empty if AP has no password

// Default IP and HTTP port of the laser ESP8266 Access Point
const char*   LASER_HOST  = "192.168.4.1";
const uint16_t LASER_PORT = 80;

// ---------------------------------------------------------------------------
// Optional MARK button (future use)
// ---------------------------------------------------------------------------
// Connect a push button between D3 (GPIO0) and GND if you want to use it.
// It can be used later to send a /mark command to the laser board.
#define PIN_MARK_BUTTON D3  // GPIO0, button to GND (INPUT_PULLUP)

// ---------------------------------------------------------------------------
// UI state machine
// ---------------------------------------------------------------------------
enum UiState {
  BOOT_LOGO,   // Show boot logo and project info
  CONNECTING,  // Show "connecting" screen and try WiFi
  RUNNING      // Show live distance
};

UiState uiState = BOOT_LOGO;
unsigned long stateStartMs = 0;

// Last distance from laser in millimeters
long lastDistanceMm = -1;
unsigned long lastDataMs = 0;

// Timing constants
const unsigned long BOOT_LOGO_DURATION_MS   = 6000; // 6 seconds
const unsigned long CONNECT_ANIM_INTERVAL_MS = 500; // 0.5 seconds
const unsigned long WIFI_STATUS_INTERVAL_MS   = 3000; // 3 seconds
const unsigned long LASER_POLL_INTERVAL_MS    = 500;  // 0.5 seconds
const unsigned long LASER_TIMEOUT_MS          = 5000; // 5 seconds

// ---------------------------------------------------------------------------
// Helper: draw "AMIR" logo at the top center
// ---------------------------------------------------------------------------
void drawLogo() {
  const char* logoText = "AMIR Mobasheraghdam";

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(logoText, 0, 0, &x1, &y1, &w, &h);

  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = 0;

  display.setCursor(x, y);
  display.print(logoText);

  // underline under the logo (full width)
  display.drawLine(0, h + 2, SCREEN_WIDTH - 1, h + 2, SSD1306_WHITE);
}

// ---------------------------------------------------------------------------
// UI: show big distance (RUNNING)
// ---------------------------------------------------------------------------
void showDistance(long mm) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  if (mm < 0) {
    // No valid data received yet
    display.setCursor(20, 24);
    display.print("NO DATA");
  } else {
    float meters = mm / 1000.0f; // convert millimeters to meters

    // Prepare text, for example: "12.345"
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3f", meters);

    // Center the text horizontally
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (SCREEN_WIDTH - w) / 2;
    int16_t y = 20;

    display.setCursor(x, y);
    display.print(buf);

    // Small "m" unit on the right side
    display.setTextSize(1);
    display.setCursor(SCREEN_WIDTH - 20, y + 4);
    display.print("m");
  }

  display.display();
}

// ---------------------------------------------------------------------------
// UI: boot logo (first seconds after power-up)
// ---------------------------------------------------------------------------
void showBootLogo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Top "AMIR" logo
  drawLogo();

  // Project title
  display.setTextSize(1);
  display.setCursor(10, 28);
  display.print("Receiver_ESP826 Receiver");

  // Author line – full name for SEO and credits
  display.setCursor(5, 46);
  display.print("by Amir Mobasheraghdam");

  display.display();
}

// ---------------------------------------------------------------------------
// UI: connecting screen with simple animation
// ---------------------------------------------------------------------------
void showConnecting() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Top "AMIR" logo
  drawLogo();

  // Animated dots
  static uint8_t dots = 0;
  dots = (dots + 1) % 4; // cycles 0..3

  display.setTextSize(2);
  display.setCursor(8, 26);
  display.print("CONNECT");

  display.setCursor(32, 46);
  display.print("ING");

  // Small dots under "ING"
  display.setCursor(100, 46);
  for (uint8_t i = 0; i < dots; i++) {
    display.print(".");
  }

  display.display();
}

void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // already connected
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi AP: ");
  Serial.println(WIFI_SSID);
}

// ---------------------------------------------------------------------------
// HTTP GET /json from laser board
// The laser board should return JSON including a numeric field "mm".
// Example:
//   {"mm":12345, "status":"OK"}
//
// Returns true if "mm" value was successfully parsed.
// ---------------------------------------------------------------------------
bool fetchDistanceFromLaser(long &mm) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClient client;
  if (!client.connect(LASER_HOST, LASER_PORT)) {
    Serial.println("Connection to laser board failed");
    return false;
  }

  // Simple HTTP GET request
  client.print(
    String("GET /json HTTP/1.1\r\n") +
    "Host: " + LASER_HOST + "\r\n" +
    "Connection: close\r\n\r\n"
  );

  // Wait for response (up to 1 second)
  unsigned long startMs = millis();
  while (!client.available() && millis() - startMs < 1000) {
    delay(10);
  }

  if (!client.available()) {
    Serial.println("No data from laser board");
    client.stop();
    return false;
  }

  // Read the entire HTTP response into a string
  String payload;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    payload += line;
  }
  client.stop();

  // Find "mm" field in JSON
  int idx = payload.indexOf("\"mm\":");
  if (idx < 0) {
    Serial.println("Field \"mm\" not found in JSON response");
    Serial.println(payload);
    return false;
  }

  int start = idx + 5; // position after "mm":
  int end = start;
  while (end < (int)payload.length() &&
         (isdigit(payload[end]) || payload[end] == '-')) {
    end++;
  }

  String numStr = payload.substring(start, end);
  mm = numStr.toInt();

  Serial.print("Distance from laser (mm) = ");
  Serial.println(mm);

  return true;
}

// ---------------------------------------------------------------------------
// setup() Amir Mobasheraghdam IUT university 
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Initialize MARK button (optional)
  pinMode(PIN_MARK_BUTTON, INPUT_PULLUP);

  // Initialize I2C and OLED display
  Wire.begin(D2, D1); // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // typical OLED address 0x3C
    // If you see nothing, try address 0x3D
    Serial.println("SSD1306 allocation failed");
    for (;;) {
      // Stop here if display initialization failed
    }
  }

  // Show boot logo and set initial state
  showBootLogo();
  stateStartMs = millis();
  uiState = BOOT_LOGO;

  // Prepare WiFi (we start connecting after the boot logo)
  WiFi.mode(WIFI_STA);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  switch (uiState) {

    case BOOT_LOGO:
      // Stay on boot logo for a fixed duration
      if (now - stateStartMs >= BOOT_LOGO_DURATION_MS) {
        uiState = CONNECTING;
        stateStartMs = now;
        ensureWifiConnected();
        showConnecting();
      }
      break;

    case CONNECTING: {
      // Retry WiFi connection if needed
      if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastPrint = 0;
        if (now - lastPrint > WIFI_STATUS_INTERVAL_MS) {
          Serial.println("Waiting for WiFi connection to laser board...");
          lastPrint = now;
        }
      } else {
        // WiFi is connected, try to get a distance once
        long mm;
        if (fetchDistanceFromLaser(mm)) {
          lastDistanceMm = mm;
          lastDataMs = now;
          uiState = RUNNING;
          showDistance(lastDistanceMm);
        }
      }

      // Refresh the "Connecting..." animation
      static unsigned long lastAnim = 0;
      if (now - lastAnim > CONNECT_ANIM_INTERVAL_MS) {
        showConnecting();
        lastAnim = now;
      }
      break;
    }

    case RUNNING: {
      // Periodically request new distance from laser board
      if (now - lastDataMs > LASER_POLL_INTERVAL_MS) {
        long mm;
        if (fetchDistanceFromLaser(mm)) {
          lastDistanceMm = mm;
          lastDataMs = now;
          showDistance(lastDistanceMm);
        } else {
          // If no valid data for too long, go back to CONNECTING state
          if (now - lastDataMs > LASER_TIMEOUT_MS) {
            uiState = CONNECTING;
            stateStartMs = now;
            showConnecting();
          }
        }
      }

      // Optional MARK button logic (not implemented yet)
      /*
      if (digitalRead(PIN_MARK_BUTTON) == LOW) {
        // TODO: send HTTP GET /mark to laser board
        // Example idea:
        //   WiFiClient client;
        //   if (client.connect(LASER_HOST, LASER_PORT)) {
        //     client.print("GET /mark HTTP/1.1\r\nHost: ");
        //     client.print(LASER_HOST);
        //     client.print("\r\nConnection: close\r\n\r\n");
        //     client.stop();
        //   }
      }
      */
      break;
    }
  }
}
