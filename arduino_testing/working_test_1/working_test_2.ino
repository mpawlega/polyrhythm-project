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
  FastLED.setBrightness(128); // default brightness
}

void loop() {
   if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();  // remove whitespace or \r

    if (input == "BounceAnimation") {
      BounceAnimation();
    } else if (input == "BurstAnimation") {
      BurstAnimation(LEDIndex, coloursArray, brightness);
    }
    // how to clear the last effect before triggering another function? 
  }
}


// Functions
void BounceAnimation() {
  for (int i = 0; i <= NUM_LEDS; i++) {
    leds[i] = CHSV(Colour1, Saturation, brightness);
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;
  }

  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    leds[i] = CHSV(Colour2, Saturation, brightness);
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;

    // colour variation, speed, length for the bounce?
  }
}

void BurstAnimation(int ledIndex, int* coloursArray, int brightness){
  for (int i = 0; i <= maxDistance; i++) {
    int right = ledIndex + i;
    int left = ledIndex - i;

    leds[right] = CHSV(coloursArray[i], Saturation, brightness); // CHSV uses Hue/Saturation/Value
    leds[left] = CHSV(coloursArray[i], Saturation, brightness); 

    delay(500); 
    FastLED.show();
    //leds[i] = CRGB::Black;

    // clear as it goes or clear after it completes the burst? 
    // how does brightness vary as it bursts? 
    // what colour variation in this case?
    // how large should the burst be?
    // edge cases so you can't create a burst near the edge?
    // how fast is the burst?

    }
}


