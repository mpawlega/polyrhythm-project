#include <FastLED.h>

#define LED_PIN     6 // define the pin the LED is connected to
#define NUM_LEDS    30
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
CRGB leds[NUM_LEDS];

void setup() { // used to setup LED pin outputs, serial connections, input/outputs
  Serial.begin(9600); // baud rate same as PD, sets connection between Arduino and other devices
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(128); // default brightness
  fill_solid(leds, NUM_LEDS, CRGB::Blue); // or your color
  FastLED.show();
}

void loop() {
  if (Serial.available() > 0) {
    int brightness = Serial.read(); // read 0–255
    brightness = constrain(brightness, 0, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}


