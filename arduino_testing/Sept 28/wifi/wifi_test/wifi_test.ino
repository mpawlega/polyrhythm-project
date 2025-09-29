#include <ESP8266WiFi.h>
#include <FastLED.h>

#define NUM_LEDS 37
#define DATA_PIN D4
#define CLOCK_PIN 13
int delay_time = 100;
CRGB leds[NUM_LEDS];

const char* ssid = "matilda";
const char* password = "D1minimini";

void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
  FastLED.clear();
  FastLED.show();
  Serial.begin(115200);
  delay(100); // allow Serial to start




  Serial.println();
  Serial.println("Booting D1 Mini...");

  WiFi.mode(WIFI_STA);          // Station mode only
  WiFi.setAutoReconnect(true);  // enable automatic reconnect
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
  for (int i = 0; i < NUM_LEDS - 3; i++){ 
    if (i == 0){
      leds[i] = CRGB::White;
      leds[i+1] = CRGB::White;
      leds[i+2] = CRGB::White;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();

    } else {
        leds[i] = CRGB::Purple;
        leds[i+1] = CRGB::Red;
        leds[i+2] = CRGB::Purple;
        FastLED.show();
        delay(delay_time);
        FastLED.clear();
    } 
  }
  
  for (int i = NUM_LEDS - 3; i > 0; i--){
    if (i == NUM_LEDS - 3){
      leds[i] = CRGB::White;
      leds[i+1] = CRGB::White;
      leds[i+2] = CRGB::White;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();

    } else {
      leds[i] = CRGB::Purple;
      leds[i+1] = CRGB::Red;
      leds[i+2] = CRGB::Purple;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();
    }
  }
}


