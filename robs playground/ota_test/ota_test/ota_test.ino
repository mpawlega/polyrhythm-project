#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

// Wi-Fi credentials
const char* ssid = "MargaretAve";
const char* password = "robmaudamelinesimonluc";

void setup() {
  Serial.begin(115200);
  delay(1000);  // let serial start
  Serial.println("\nBooting ESP...");

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // --- OTA Setup ---
  ArduinoOTA.setHostname("D1mini_OTA");

  // OTA callbacks
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA End");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\nOTA Error[%u]: ", error);
    switch (error) {
      case OTA_AUTH_ERROR:   Serial.println("Auth Failed"); break;
      case OTA_BEGIN_ERROR:  Serial.println("Begin Failed"); break;
      case OTA_CONNECT_ERROR:Serial.println("Connect Failed"); break;
      case OTA_RECEIVE_ERROR:Serial.println("Receive Failed"); break;
      case OTA_END_ERROR:    Serial.println("End Failed"); break;
      default:               Serial.println("Unknown Error"); break;
    }
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

void loop() {
  // Must be called frequently
  ArduinoOTA.handle();

  // Debug: show loop is running and OTA is active
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    Serial.println("Loop ticking, OTA handler active");
    lastPrint = millis();
  }
}
