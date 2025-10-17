#include <FastLED.h>

#define NUM_LEDS 130
#define DATA_PIN D4
#define DATA_PIN_MIDDLE D5
#define CLOCK_PIN 13

int delay_time = 50;
int bounce_delay_time = 75;
int trail_length = 15;
int bounce_length = 30; 

CRGB leds[NUM_LEDS];
CRGB ledsMiddle[NUM_LEDS];

void setup() { 
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_MIDDLE>(ledsMiddle, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
}

void loop() { 

  int brightness[15] = {255, 238, 221, 204, 187, 170, 153, 136, 119, 102, 85, 68, 51, 38, 24};
  
  // Forwards
  for (int i = 0; i < NUM_LEDS - 3; i++) { 
    if (i == 0) {
      for (int j = 0; j < bounce_length; j++) {
        leds[j] = CHSV(213, 200, brightness[j]);
        ledsMiddle[j] = CHSV(213, 200, brightness[j]);
        FastLED.show();
        //delay(bounce_delay_time);
      }
      delay(bounce_delay_time);
      FastLED.clear();
    }
      leds[i] = CHSV(192, 200, 32);
      leds[i+1] = CHSV(192, 255, 128);
      leds[i+2] = CHSV(192, 255, 128);
      leds[i+3] = CHSV(192, 255, 32);

      ledsMiddle[i]   = CHSV(192, 255, 128);
      ledsMiddle[i+1] = CHSV(213, 255, 255);
      ledsMiddle[i+2] = CHSV(213, 255, 255);
      ledsMiddle[i+3] = CHSV(192, 255, 128);

    // Trail
      for (int k = 1; k <= trail_length; k++){
        if (i >= k){
          float spacing = (32 - 10) / (trail_length - 1);
          int brightness = 32 - spacing * (k - 1);
          leds[i-k] = CHSV(200, 200, brightness);
          ledsMiddle[i-k] = CHSV(200, 200, brightness);
        }
      }
      FastLED.show();
      delay(delay_time);
      FastLED.clear();
  }

  // Backwards
  for (int i = NUM_LEDS - 3; i > 0; i--) { // 37 - 3  = 34 to less than zero, 34,33,32,31,30, ... 1
    if (i == NUM_LEDS - 3) {
      int increment = 0;
      for (int j = NUM_LEDS - 1; j > NUM_LEDS - 1 - bounce_length; j--) { 
        leds[j] = CHSV(213, 200, brightness[increment]);  //  36,35,34,33,32,31,30,29,28,27,26,25,24,23,22
        ledsMiddle[j] = CHSV(213, 200, brightness[increment]);  // 28 and 29 have an issue
        FastLED.show();   // starts at 37 - 3 = 34, so when i = 34, go to bounce mode, then you 37-1 = 36 
        //delay(bounce_delay_time);
        increment++; 
      }
      delay(bounce_delay_time);
      FastLED.clear();
    }
      leds[i] = CHSV(192, 200, 32);
      leds[i+1] = CHSV(192, 255, 128);
      leds[i+2] = CHSV(192, 255, 32);

      ledsMiddle[i]   = CHSV(192, 255, 128);
      ledsMiddle[i+1] = CHSV(213, 255, 255);
      ledsMiddle[i+2] = CHSV(192, 255, 128);

    // Trail
      for (int k = 3; k <= trail_length + 2; k++){
        if (int (i+k) > 36){ // change numleds-1
          leds[i+k] = CHSV(200, 200, 0); 
          ledsMiddle[i+k] = CHSV(200, 200, 0);
        } else{
          float spacing = (32 - 10) / (trail_length - 1);
          int brightness = 32 - spacing * (k - 3);
          leds[i+k] = CHSV(200, 200, brightness); //343536,
          ledsMiddle[i+k] = CHSV(200, 200, brightness);
        }
      }
      FastLED.show();
      delay(delay_time);
      FastLED.clear();
  }
}
