// Libraries
#include <FastLED.h>
#define NUM_LEDS 37
#define DATA_PIN 6

// Variables
int initial_brightness = 100;
int initial_speed = 100; 
int initial_colour =100;
int initial_saturation = 255;
CRGB leds[NUM_LEDS]; // LED array

void setup() {
  Serial.begin(19200); // match with PureData baud rate
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.setBrightness(100); // default brightness
  //fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
}




char currentState = 'C'; 
int param = 100;         

void loop() {
  while (Serial.available() > 0) {
    int incoming = Serial.read();

    if (incoming == 'A' || incoming == 'B' || incoming == 'C') {
      currentState = (char)incoming;
    } else {
      param = incoming;
    }
  }

  switch (currentState) {
    case 'A':
      Green(param); 
      break;
    case 'B':
      stop();     
      break;
    case 'C':
      idle();      
      break;
  }
}
 
void Green(int brightness){
  leds[0] = CHSV(96, initial_saturation, brightness);
  FastLED.show();
}

void firstgreen(){
  leds[0] = CHSV(96, initial_saturation, initial_brightness);
  FastLED.show();
}

void stop(){
  FastLED.clear();
  FastLED.show();
}

void idle(){
  fill_solid(leds, NUM_LEDS, CRGB::Blue);
  FastLED.show();
}

/*

    if (function_trigger == 'A'){
      firstgreen();
    }

    else if (function_trigger = 'B'){
      stop();
    }

*/

/*
void Red(int brightness, int speed){
  leds[0] = CHSV(96, saturation, brightness);
  FastLED.show();
  delay(speed);
}
*/

// how do you remember the previous state if it's already deleted by serial read when you restart?
// idle mode


// three thigns happening here, i trigger func1 or func2, then deliver brightness and speed
// how is this working? the speed, brightness i want updated in real time
// the function trigger i want to run then ovveride
// then i need to understand how i am sending the data, because PD is not feeding in parameters, its just sending bytes
// arduino actually parses the data
// so I need to either send a packaged array of data values, or trigger them separately
// in that case, need to be aware of baud rate, and packge data as either ascii and convert to char, or process the int, and find a way to do it in real time
// i want the entire strip cleared when the next function is triggered 


// it's sending in order, so would a package be better in that case? 

// pack func1 func2 brightness speed 

/*switch(function_trigger){
      case 'A':
        Green(brightness_val, speed_val);
        break;
      case 'B':
        Red(brightness_val, speed_val);
        break;
      case 'C':
        stop();
        break;
    }*/


/* //else if (function_trigger == 'B'){
      Red(brightness_val, speed_val);
      }
    //else if (function_trigger == 'C'){
      stop();
      }*/