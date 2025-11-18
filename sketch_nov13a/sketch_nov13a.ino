#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>

// ===================== WiFi Config =====================
const char* ssid = "itik";
const char* password = "cucutimah";

// ===================== Web Server =====================
ESP8266WebServer server(80);

// ===================== GPS Setup =====================
#define GPS_RX D2
#define GPS_TX D1
SoftwareSerial GPS(GPS_RX, GPS_TX);
TinyGPSPlus gps;

// ===================== SOS Button =====================
#define SOS_PIN D3
const unsigned long SOS_DEBOUNCE_MS = 200;

// ===================== Timing & Geofence =====================
const unsigned long UPDATE_INTERVAL_MS = 45000;  // 45s
const unsigned long GEOFENCE_ALERT_MIN_INTERVAL = 5000; // 5s
const double GEOFENCE_LAT = 2.9040120577961455;
const double GEOFENCE_LON = 101.8644031932005;
const double GEOFENCE_RADIUS_M = 50.0; // ✅ tukar ke 50 meter

// ===================== State =====================
double currentLat = 0.0;
double currentLon = 0.0;
String lastTitle = "No Data Yet";
String lastBody = "Waiting for GPS...";
unsigned long lastUpdate = 0;
unsigned long lastGeofenceAlertMillis = 0;
unsigned long lastSOSMillis = 0;

// ===================== Function Prototypes =====================
void handleRoot();
void handleData();
double haversine(double lat1, double lon1, double lat2, double lon2);
void checkGeofence();
void ensureWiFiConnected();

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  GPS.begin(9600);
  pinMode(SOS_PIN, INPUT_PULLUP); // tekan ke GND

  Serial.println("\n=== ESP32 GPS Tracker Web Server ===");
  ensureWiFiConnected();

  Serial.print("✅ ESP32 IP (akses di browser): ");
  Serial.println(WiFi.localIP());
  Serial.println("---------------------------------------");

  // Web routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("🌐 Web server started on port 80.");
}

// ===================== LOOP =====================
void loop() {
  server.handleClient();

  // Baca GPS data
  while (GPS.available() > 0) gps.encode(GPS.read());

  if (gps.location.isValid()) {
    currentLat = gps.location.lat();
    currentLon = gps.location.lng();

    unsigned long now = millis();
    if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
      lastUpdate = now;
      lastTitle = "PERIODIC UPDATE";
      lastBody = "Location updated every 45 seconds.";
      Serial.printf("📍 Lat: %.6f, Lon: %.6f\n", currentLat, currentLon);
    }

    checkGeofence();
  } else {
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 5000) {
      Serial.println("Waiting for GPS fix...");
      lastMsg = millis();
    }
  }

  // Butang SOS
  static int lastSOSState = HIGH;
  int s = digitalRead(SOS_PIN);
  if (s == LOW && lastSOSState == HIGH) {
    unsigned long now = millis();
    if (now - lastSOSMillis > SOS_DEBOUNCE_MS) {
      lastSOSMillis = now;
      lastTitle = "🚨 SOS ALERT";
      lastBody = "SOS button pressed!";
      Serial.println("🚨 SOS ALERT TRIGGERED!");
    }
  }
  lastSOSState = s;
}

// ===================== HANDLE ROOT =====================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='5'>";
  html += "<title>ESP32 GPS Tracker</title>";
  html += "<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;} ";
  html += ".card{background:#222;padding:20px;border-radius:10px;width:80%;margin:auto;box-shadow:0 0 10px #444;} ";
  html += "h1{color:#0ff;} .alert{color:#f66;font-weight:bold;}</style></head><body>";
  html += "<h1>📡 ESP32 GPS Tracker Dashboard</h1>";
  html += "<div class='card'>";
  html += "<h2>" + lastTitle + "</h2>";
  html += "<p>" + lastBody + "</p>";
  html += "<p><b>Latitude:</b> " + String(currentLat, 6) + "</p>";
  html += "<p><b>Longitude:</b> " + String(currentLon, 6) + "</p>";
  html += "<p><a href='https://maps.google.com/?q=" + String(currentLat, 6) + "," + String(currentLon, 6) + "' target='_blank'>Open in Google Maps</a></p>";
  html += "<p>ESP32 IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p>Auto refresh every 5 seconds</p>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// ===================== HANDLE /data (API) =====================
void handleData() {
  String json = "{";
  json += "\"title\":\"" + lastTitle + "\",";
  json += "\"body\":\"" + lastBody + "\",";
  json += "\"lat\":" + String(currentLat, 6) + ",";
  json += "\"lon\":" + String(currentLon, 6);
  json += "}";
  server.send(200, "application/json", json);
}

// ===================== WIFI =====================
void ensureWiFiConnected() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
}

// ===================== GEOFENCE =====================
void checkGeofence() {
  double dist = haversine(currentLat, currentLon, GEOFENCE_LAT, GEOFENCE_LON);
  unsigned long now = millis();
  if (dist > GEOFENCE_RADIUS_M && (now - lastGeofenceAlertMillis > GEOFENCE_ALERT_MIN_INTERVAL)) {
    lastGeofenceAlertMillis = now;
    lastTitle = "🚧 GEOFENCE ALERT";
    lastBody = "Child exits the boundary!"; // ✅ mesej yang awak mahu
    Serial.printf("⚠️ OUT OF BOUNDARY (%.2f m)\n", dist);
  }
  else if (dist <= GEOFENCE_RADIUS_M && (now - lastGeofenceAlertMillis > GEOFENCE_ALERT_MIN_INTERVAL)) {
    lastTitle = "✅ SAFE ZONE";
    lastBody = "Child is within the boundary."; // paparan bila masih dalam sempadan
    Serial.printf("🟢 Within boundary (%.2f m)\n", dist);
  }
}

// ===================== HAVERSINE =====================
double haversine(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0; // jejari bumi (m)
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}
