/// @file    arduinotest.ino
/// @brief   Send bounce from the start to end of the LED and bounce back.
/// @example Blink.ino

// pins and libraries
#include <FastLED.h>
#define NUM_LEDS 37
#define DATA_PIN 6
#define CLOCK_PIN 13

// variables
int Colour1 = 192; 
int Colour2 = 160; 
int Saturation = 255; 
int brightness = 255; // to be changed in loop
int Colour = 100; // from serial
int maxDistance = 6; // need edge case

// delcares array of CRBG objects, to address each LED and assign in RGB
CRGB leds[NUM_LEDS]; 


void setup() { 
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS); 
}


void loop() { 
  burstAnimation(16);
  }


// functions


void burstAnimation(int ledIndex){
  for (int i = 0; i <= maxDistance; i++) {
    int right = ledIndex + i;
    int left = ledIndex - i;
    int newBrightness = brightness - i*2;

    leds[right] = CHSV(Colour1, Saturation, newBrightness); // CHSV uses Hue/Saturation/Value
    leds[left] = CHSV(Colour1, Saturation, newBrightness); // CHSV uses Hue/Saturation/Value


      delay(1000); // changes speed
      FastLED.show();
      //leds[i] = CRGB::Black;

      // vanish and edge detection
    }



// specify a trigger point
// at trigger point, you have lights on L/R light up that dim outwards
// do you want the middle to disappear or stay? I think stay

// leds[10] = CHSV(Colour1, Saturation, brightness);
// left 5 leds and right 5 leds out, but keep them on, each i form the central point dims
// need to fix the brightness and bounds, this doesn't work as intended



}

/* for (int i = ledIndex; i>= 0; i--) {
      leds[i] = CHSV(Colour1, Saturation, brightness - i*10); // CHSV uses Hue/Saturation/Value
      delay(100);
      FastLED.show();
      //leds[i] = CRGB::Black;
    
    }
*/