#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <WiFiUdp.h>

#define NUM_LEDS 37
#define DATA_PIN D4
#define DATA_PIN_MIDDLE D5
#define CLOCK_PIN 13
int delay_time = 100;
CRGB leds[NUM_LEDS];
CRGB ledsMiddle[NUM_LEDS];
WiFiUDP Udp;
unsigned int localUdpPort = 4210;
char incomingPacket[255];      

const char* ssid = "matilda";
const char* password = "D1minimini";

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  
  FastLED.addLeds<NEOPIXEL, DATA_PIN_MIDDLE>(ledsMiddle, NUM_LEDS);
  FastLED.show();
  Serial.begin(115200);
  delay(100); 

  Serial.println();
  Serial.println("Booting D1 Mini...");

  WiFi.mode(WIFI_STA);       
  WiFi.setAutoReconnect(true); 
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

   int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect!");
  }
}

void loop() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = '\0';

    int newDelay = atoi(incomingPacket); // convert received string to int
    if (newDelay > 0 && newDelay < 5000) {
      delay_time = newDelay;
      Serial.print("New delay_time: ");
      Serial.println(delay_time);
    }
  }

  // LED behavior based on connection
  if (WiFi.status() == WL_CONNECTED) {
    leds[0] = CRGB::White;
    FastLED.show();
    delay(delay_time);   // controlled by Max now
    FastLED.clear();
  } else {
    leds[0] = CRGB::Red;
    FastLED.show();
  }
}


