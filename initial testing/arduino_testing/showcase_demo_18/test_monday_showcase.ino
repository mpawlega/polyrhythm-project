// Libraries
#include <FastLED.h>
#define NUM_LEDS 37
#define DATA_PIN 6

// Static variables
int initial_colour = 100;
int final_colour = 200;
int initial_saturation = 255;
CRGB leds[NUM_LEDS]; 

// Update variables
int brightness = 100;
int speed = 100;
int colour = 0; // red for first index of CHSV format 
int LEDIndex = 18;
int maxDistance = 6;
// int spacing = 2;
// coloursArray
char current_state = 'C'; //idle state to be updated in the loop

void setup(){
  Serial.begin(19200); // match with PureData baud rate
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.setBrightness(100); // default brightness
  //fill_solid(leds, NUM_LEDS, CRGB::White);
  //FastLED.clear();
  FastLED.show();
}


void loop() {
  if (Serial.available() > 0) {
    char trigger = Serial.read();

    if (trigger == 'A' && Serial.available() > 0) brightness = Serial.read(); // 65
    else if (trigger == 'B' && Serial.available() > 0) speed = Serial.read(); // 66
    else if (trigger == 'C' && Serial.available() > 0) colour = Serial.read(); // 67
    // else if (trigger == 'D' && Serial.available() > 0) colourArray = Serial.read(); // 68
    else if (trigger == 'E' && Serial.available() > 0) LEDIndex = Serial.read(); // 69
    else if (trigger == 'F' && Serial.available() > 0) maxDistance = Serial.read(); // 70
    // else if (trigger == 'G' && Serial.available() > 0) spacing = Serial.read(); // 71
    else current_state = trigger;
  }

  trigger_state(current_state); 
  delay(100);
}

void green(int brightness){
  FastLED.clear();
  leds[0] = CHSV(96, initial_saturation, brightness);
  FastLED.show();
}

void red(int brightness){
  FastLED.clear();
  leds[0] = CHSV(0, initial_saturation, brightness);
  FastLED.show();
}

void idle(){
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
}

void pause(){
  FastLED.clear();
}

void reset(){
  // todo
}

void trigger_state(char state){
  switch(state){
      case 'H': // 72 
        BounceAnimation(brightness, speed, colour); break;
      case 'I': // 73
        red(brightness); break;
      case 'J': // 74
        BurstAnimation(brightness, speed, colour, LEDIndex, maxDistance); break;
      case 'K': // 75
        pause(); break; // double check, it pauses but code clears, and restart in the function or elsewhere? it may because I didn't do FastLED.show();
      case 'L':
        reset(); break;
      default: idle(); break; // after how long does it go idle? 
  }
}

void BounceAnimation(int brightness, int speed, int colour){
  static unsigned long previous_bounce_millis = 0;
  unsigned long current_bounce_millis = millis();
  static int bounceIndex = 0;
  static int bounceDir = 1;

  if (current_bounce_millis - previous_bounce_millis >= speed){
    previous_bounce_millis = current_bounce_millis;
    FastLED.clear();

    leds[bounceIndex] = CHSV(colour, initial_saturation, brightness);
    FastLED.show();

    bounceIndex += bounceDir;

    if (bounceIndex >= NUM_LEDS || bounceIndex < 0){
      bounceDir = - bounceDir;
      bounceIndex += bounceDir;
    }
  }
}


void BurstAnimation(int brightness, int speed, int colour, int LEDIndex, int maxDistance){
  static unsigned long previous_millis = 0;
  unsigned long current_millis = millis();
  static int step = 0;

  if (current_millis - previous_millis >= speed){
    previous_millis = current_millis;
    // FastLED.clear(); // may remove
    // check so it doesn't go out of bounds? 

    int right = LEDIndex + step; 
    int left = LEDIndex - step; 

    leds[left] = CHSV(colour, initial_saturation, brightness); 
    leds[right] = CHSV(colour, initial_saturation, brightness); 

    FastLED.show();

    step++;

    if (step > maxDistance){
      step = 0;
      // running = false; 
      // FastLED.clear();
      // FastLED.show(); 
    }
  }
}


// test 1
/*
void setup() {
  Serial.begin(19200); // match with PureData baud rate
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.setBrightness(100); // default brightness
  //fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
}

void loop(){
  while (Serial.available() >= 2){
    char trigger = Serial.read();
    int variable = Serial.read();

    if (trigger == 'B') brightness = variable;
    else if (trigger == 'S') speed = variable;
    else current_state = trigger;
    
trigger_state(current_state);
  }
}
*/


// test 2
/*
void loop() {
  if (Serial.available() > 0) {
    char trigger = Serial.read();

    switch(trigger) {
      case 'B': // brightness update
        if (Serial.available() > 0) {
          brightness = Serial.read();
        }
        break;

      case 'S': // speed update
        if (Serial.available() > 0) {
          speed = Serial.read();
        }
        break;

      case 'A': // green
      case 'R': // red
      case 'C': // idle
        current_state = trigger;
        break;

      default:
        current_state = 'X'; // unknown
        break;
    }
  }

  trigger_state(current_state); // update LEDs once per loop
  delay(50);
}
*/








/*
this code works and just filters through different modes, no variable intake for functions yet

void loop(){
  if (Serial.available() > 0){
    char trigger = Serial.read();
    int value = Serial.read();

    switch(trigger){
      case 'A':
      // 65
        green();
        break;
      case 'B':
      // 66
        red();
        break;
      case 'C':
      // 67
        idle();
        break;
      default:
        clear();
        break;
    }
  }
}
*/




