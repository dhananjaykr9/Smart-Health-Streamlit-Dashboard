#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// Wi-Fi credentials
#define WIFI_SSID "WIFI_Name"
#define WIFI_PASSWORD "WIFI_Password"

// Firebase credentials
#define API_KEY "YOUR_API_KEY"
#define DATABASE_URL "YOUR_DATABASE_URL"
#define USER_EMAIL "YOUR_FIREBASE_EMIAL"
#define USER_PASSWORD "EMAIL_PASSWORD"

// DS18B20 setup
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000;  // 10 seconds

// Wait for valid NTP time with timeout
void waitForNTP() {
  Serial.print("⏳ Waiting for NTP time");
  time_t now;
  int retries = 0;
  do {
    now = time(nullptr);
    Serial.print(".");
    delay(500);
    retries++;
    if (retries > 30) {
      Serial.println("\n❌ NTP sync failed — restarting");
      ESP.restart();
    }
  } while (now < 1700000000);
  Serial.println("\n✅ Time synchronized!");
}

void setup() {
  Serial.begin(115200);
  sensors.begin();

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++retries > 40) {
      Serial.println("\nWiFi failed — restarting");
      ESP.restart();
    }
  }
  Serial.println("\n✅ WiFi connected. IP: " + WiFi.localIP().toString());

  // Set IST Timezone
  delay(2000);
  configTzTime("IST-5:30", "1.in.pool.ntp.org", "1.asia.pool.ntp.org", "time.nist.gov");


  waitForNTP();

  // Firebase setup
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  time_t now = time(nullptr);
  
  Serial.printf("Current epoch time: %ld\n", now);
  // If time becomes invalid again (e.g., ESP32 lost sync), re-sync
  if (now < 1700000000) {
    Serial.println("⚠️ Time invalid again — re-syncing NTP");
    configTzTime("IST-5:30", "1.in.pool.ntp.org", "1.asia.pool.ntp.org", "time.nist.gov");
    waitForNTP();  // will restart if it fails
    return;
  }

  if (Firebase.ready() && (millis() - lastSendTime > sendInterval)) {
    lastSendTime = millis();

    struct tm *timeinfo = localtime(&now);

    char dateFolder[20];
    strftime(dateFolder, sizeof(dateFolder), "%Y-%m-%d", timeinfo);

    String key = String(now);
    String path = "/sensor_data/" + String(dateFolder) + "/" + key;

    int HeartRate = random(60, 120);
    int Sp02 = random(90,100);
    float BodyTemp = random(360, 390) / 10;
    float RoomTemp = random(250, 320) / 10.0;
    int Humidity = random(30,70);

    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);

    FirebaseJson json;
    json.set("Time", String(timeStr));
    json.set("HeartRate", HeartRate);
    json.set("Sp02", Sp02);
    json.set("BodyTemp", BodyTemp);
    json.set("RoomTemp", RoomTemp);
    json.set("Humidity", Humidity);
     

    Serial.println("Sending to Firebase:");
    json.toString(Serial, true);

    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json))
      Serial.println("\n✅ Data sent successfully.");
    else
      Serial.printf("\n❌ Failed: %s\n", fbdo.errorReason().c_str());
  }
}
