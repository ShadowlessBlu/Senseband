#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketServer.h>
#include <Adafruit_ADXL345_U.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <FS.h>
#include <SPIFFS.h>
#include <esp_sleep.h>

// WiFi Credentials
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Web Server and WebSocket
WebServer server(80);
WebSocketServer webSocketServer;
const char* websocket_path = "/ws";

// ADXL345
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// GPS
TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // RX: GPIO4, TX: GPIO5
// GSM
SoftwareSerial gsmSerial(6, 7); // RX: GPIO6, TX: GPIO7
const char* alertNumber = "+1234567890"; // Emergency contact number
// Pins
#define VIBRATION_PIN 8 // PWM pin for vibration motor
#define SOS_BUTTON 9    // SOS button pin
#define BATTERY_PIN 2   // ADC pin for battery voltage

// Geofence
const float GEOFENCE_LAT = 37.7749; // Center latitude
const float GEOFENCE_LON = -122.4194; // Center longitude
const float GEOFENCE_RADIUS = 100.0; // Radius in meters

// Battery
const float BATTERY_MIN_V = 3.3; // Minimum voltage (LiPo)
const float BATTERY_MAX_V = 4.2; // Maximum voltage
const float LOW_BATTERY_THRESHOLD = 3.5;

// Sleep
#define SLEEP_DURATION 30000000 // 30 seconds in microseconds

// Vibration Patterns
struct VibrationPattern {
  int durationOn;
  int durationOff;
  int repeat;
};

VibrationPattern fallPattern = {200, 200, 3}; // Short bursts for fall
VibrationPattern sosPattern = {500, 500, 5};  // Longer bursts for SOS

// Global Variables
bool fallDetected = false;
bool sosTriggered = false;
bool outsideGeofence = false;
float batteryVoltage = 0.0;
bool isMoving = false;
unsigned long lastMovementTime = 0;
String lastFallTime = "--";
String lastSosTime = "--";

// Fall Detection
bool detectFall() {
  sensors_event_t event;
  accel.getEvent(&event);

  // Free-fall detection: low total acceleration
  float totalAccel = sqrt(event.acceleration.x * event.acceleration.x +
                          event.acceleration.y * event.acceleration.y +
                          event.acceleration.z * event.acceleration.z);
  if (totalAccel < 0.5) { // Free-fall threshold
    delay(100); // Wait to confirm
    accel.getEvent(&event);
    totalAccel = sqrt(event.acceleration.x * event.acceleration.x +
                      event.acceleration.y * event.acceleration.y +
                      event.acceleration.z * event.acceleration.z);
    if (totalAccel > 10.0) { // Impact threshold
      lastFallTime = String(gps.date.year()) + "-" + String(gps.date.month()) + "-" + String(gps.date.day()) + " " +
                     String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
      return true;
    }
  }
  return false;
}

// GPS Distance Calculation
float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  float dLat = radians(lat2 - lat1);
  float dLon = radians(lon2 - lon1);
  float a = sin(dLat / 2) * sin(dLat / 2) +
            cos(radians(lat1)) * cos(radians(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return 6371000 * c; // Distance in meters
}

// Geofence Check
bool checkGeofence(float lat, float lon) {
  float distance = calculateDistance(lat, lon, GEOFENCE_LAT, GEOFENCE_LON);
  return distance <= GEOFENCE_RADIUS;
}

// GSM Alert
void sendSMSAlert(String message) {
  gsmSerial.println("AT+CMGF=1"); // Set SMS text mode
  delay(100);
  gsmSerial.println("AT+CMGS=\"" + String(alertNumber) + "\"");
  delay(100);
  gsmSerial.print(message);
  delay(100);
  gsmSerial.write((char)26); // End SMS
  delay(1000);
}

// Vibration
void vibrate(VibrationPattern pattern) {
  for (int i = 0; i < pattern.repeat; i++) {
    analogWrite(VIBRATION_PIN, 255); // Full intensity
    delay(pattern.durationOn);
    analogWrite(VIBRATION_PIN, 0);
    delay(pattern.durationOff);
  }
}

// Battery Monitoring
float readBatteryVoltage() {
  int adcValue = analogRead(BATTERY_PIN);
  float voltage = (adcValue / 4095.0) * 3.3 * 2; // Voltage divider
  return voltage;
}

// Battery Percentage
int voltageToPercentage(float voltage) {
  if (voltage >= BATTERY_MAX_V) return 100;
  if (voltage <= BATTERY_MIN_V) return 0;
  return (int)((voltage - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V) * 100);
}

// WebSocket Data
void sendWebSocketData() {
  String json = "{";
  json += "\"lat\":" + String(gps.location.isValid() ? gps.location.lat() : 0, 6) + ",";
  json += "\"lng\":" + String(gps.location.isValid() ? gps.location.lng() : 0, 6) + ",";
  json += "\"battery\":" + String(voltageToPercentage(batteryVoltage)) + ",";
  json += "\"signal\":\"--\","; // Placeholder
  json += "\"lastSync\":\"" + String(gps.date.isValid() ? String(gps.date.year()) + "-" + String(gps.date.month()) + "-" + String(gps.date.day()) + " " +
                              String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()) : "--") + "\",";
  json += "\"status\":\"" + String(isMoving ? "Moving" : "Stationary") + "\",";
  json += "\"obstacleLog\":\"" + (fallDetected ? "Fall at " + lastFallTime : "") + "\",";
  json += "\"sosLog\":\"" + (sosTriggered ? "SOS at " + lastSosTime : "") + "\",";
  json += "\"temp\":\"--\","; // Placeholder
  json += "\"humidity\":\"--\","; // Placeholder
  json += "\"rain\":\"--\""; // Placeholder
  json += "}";
  webSocketServer.sendData(json);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  // Initialize ADXL345
  if (!accel.begin()) {
    Serial.println("ADXL345 not found!");
    while (1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  
  // Initialize GPS
  gpsSerial.begin(9600);
  
  // Initialize GSM
  gsmSerial.begin(9600);
  delay(1000);
  gsmSerial.println("AT");
  delay(1000);
  
  // Initialize Pins
  pinMode(VIBRATION_PIN, OUTPUT);
  pinMode(SOS_BUTTON, INPUT_PULLUP);
  
  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  
  // Web Server
  server.on("/", []() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });
  
  server.begin();
  webSocketServer.begin(&server);
  
  // Enable Sleep Wakeup
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)SOS_BUTTON, 0);
}

void loop() {
  // Handle Web Server
  server.handleClient();
  webSocketServer.loop();
  
  // Fall Detection
  if (detectFall()) {
    fallDetected = true;
    vibrate(fallPattern);
    String message = "FALL DETECTED! Location: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    sendSMSAlert(message);
  } else {
    fallDetected = false; // Reset after alert
  }
  
  // GPS and Geofence
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        isMoving = (gps.speed.kmph() > 1.0);
        lastMovementTime = millis();
        outsideGeofence = !checkGeofence(gps.location.lat(), gps.location.lng());
        if (outsideGeofence) {
          String message = "Geofence Breach! Location: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
          sendSMSAlert(message);
        }
      }
    }
  }
  
  // SOS Button
  if (digitalRead(SOS_BUTTON) == LOW) {
    sosTriggered = true;
    lastSosTime = String(gps.date.year()) + "-" + String(gps.date.month()) + "-" + String(gps.date.day()) + " " +
                  String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
    vibrate(sosPattern);
    String message = "SOS Triggered! Location: " + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
    sendSMSAlert(message);
    delay(500); // Debounce
  } else {
    sosTriggered = false; // Reset after alert
  }
  
  // Battery Monitoring
  batteryVoltage = readBatteryVoltage();
  if (batteryVoltage < LOW_BATTERY_THRESHOLD) {
    String message = "Low Battery: " + String(batteryVoltage) + "V";
    sendSMSAlert(message);
  }
  
  // Inactivity Check
  if (millis() - lastMovementTime > 60000) { // 1 minute inactivity
    isMoving = false;
  }
  
  // Send WebSocket Updates
  sendWebSocketData();
  
  // Sleep Mode
  if (!isMoving && !fallDetected && !sosTriggered && !outsideGeofence) {
    Serial.println("Entering deep sleep...");
    esp_deep_sleep_start();
  }
  
  delay(100);
}