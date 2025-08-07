#include <FastLED.h>
#define NUM_LEDS 37
#define DATA_PIN 6

int Colour1 = 192; 
int Colour2 = 160; 
int Saturation = 255; 
int brightness = 255; 

int maxDistance = 6; 
int coloursArray[] = {
  100, 125, 150, 175, 200, 225, 250};
int LEDIndex = 16;

CRGB leds[NUM_LEDS]; // LED array

void setup() {
  Serial.begin(9600); // match with PureData baud rate
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.setBrightness(128); // default brightness
}


void loop(){
  if (Serial.available() > 0) {
    if (Serial.read() == 0) {
      Red();
      } 
    else if (Serial.read() == 1) {
     Green();
     }
}
}

void Red() {
  leds[0] = CRGB::Red;
  FastLED.show();
}

void Green() {
  leds[0] = CRGB::Green;
  FastLED.show();
}