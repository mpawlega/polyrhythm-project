#include <FastLED.h>

#define NUM_LEDS 37
#define DATA_PIN D4
#define DATA_PIN_MIDDLE D5
#define CLOCK_PIN 13
int delay_time = 100;
CRGB leds[NUM_LEDS];
CRGB ledsMiddle[NUM_LEDS];

void setup() { 
    // ## Clockless types ##
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<NEOPIXEL, DATA_PIN_MIDDLE>(ledsMiddle, NUM_LEDS);
    // FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    FastLED.clear();
    FastLED.show();
}

void loop() { 
  for (int i = 0; i < NUM_LEDS - 3; i++){ 
    if (i == 0){
      leds[i] = CRGB::White;
      leds[i+1] = CRGB::White;
      leds[i+2] = CRGB::White;
      ledsMiddle[i] = CRGB::White;
      ledsMiddle[i+1] = CRGB::White;
      ledsMiddle[i+2] = CRGB::White;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();

    } else {
        leds[i] = CRGB::Purple;
        leds[i+1] = CRGB::Purple;
        leds[i+2] = CRGB::Purple;
        ledsMiddle[i] = CRGB::Purple;
        ledsMiddle[i+1] = CRGB::Yellow;
        ledsMiddle[i+2] = CRGB::Purple;
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
      ledsMiddle[i] = CRGB::White;
      ledsMiddle[i+1] = CRGB::White;
      ledsMiddle[i+2] = CRGB::White;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();

    } else {
      leds[i] = CRGB::Purple;
      leds[i+1] = CRGB::Purple;
      leds[i+2] = CRGB::Purple;
      ledsMiddle[i] = CRGB::Purple;
      ledsMiddle[i+1] = CRGB::Yellow;
      ledsMiddle[i+2] = CRGB::Purple;
      FastLED.show();
      delay(delay_time);
      FastLED.clear();
    }
  }
}



