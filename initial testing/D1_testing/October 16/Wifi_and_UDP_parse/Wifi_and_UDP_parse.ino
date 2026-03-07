#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define NUM_LEDS 37 //change
#define DATA_PIN D4
#define DATA_PIN_MIDDLE D5
#define CLOCK_PIN 13

int tries = 0;
int max_tries = 20;

int store = 100;   
int speed = 100;   
int brightness = 100;    
int ind = 0;             
int direction = 1;    
unsigned long previousMicros = 0;

CRGB leds[NUM_LEDS];
CRGB ledsMiddle[NUM_LEDS];

// WiFi
IPAddress local_IP(192, 168, 1, 100);
IPAddress subnet(255, 255, 255, 0); 
IPAddress gateway(192, 168, 1, 1); 
IPAddress primaryDNS(192, 168, 1, 1);
//IPAddress secondaryDNS(8, 8, 4, 4); 

const char* ssid = "matilda";
const char* password = "D1minimini";

WiFiUDP Udp;
unsigned int localPort = 8888;  
char incomingPacket[255];

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_MIDDLE>(ledsMiddle, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
  Serial.println("Failed to configure static IP");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && tries < max_tries) {
    delay(1000);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED){
    Serial.println();
    Serial.print("Connected! Router IP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("Connected! D1 IP: ");
    Serial.println(WiFi.localIP());

    Udp.begin(localPort);
    Serial.print("Listening on UDP port ");
    Serial.println(localPort);
  }

  else {
    Serial.println("\n Failed after 20 tries");
  }
}

void loop() {
  //Serial.println(store);
  int packetSize = Udp.parsePacket();
    if (packetSize) {
      int len = Udp.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

      Serial.print("Received packet: ");
      Serial.println(incomingPacket);

      if (incomingPacket[0] == 'B') {
        int value = atoi(&incomingPacket[1]); // convert the rest to integer
        Serial.print("Command B detected with value: ");
        Serial.println(value);   
        store = value;
      }

      if (incomingPacket[0] == 'A') {
        int value = atoi(&incomingPacket[1]); // convert the rest to integer
        Serial.print("Command A detected with value: ");
        Serial.println(value);   
        speed = value;
      }
    }
    unsigned long currentMicros = micros();

  // Check if enough time has passed for next LED update
  if (currentMicros - previousMicros >= speed) {
    previousMicros = currentMicros;

    // Light up current LED with the stored hue
    leds[ind] = CHSV(store, 255, brightness);
    FastLED.show();
    FastLED.clear();

    // Move forward or backward
    ind += direction;
    if (ind >= NUM_LEDS - 1) direction = -1;
    if (ind <= 0) direction = 1;
  } 
}
