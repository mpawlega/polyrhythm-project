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
char current_state = 'C'; //idle state to be updated in the loop

void setup(){
  Serial.begin(19200); // match with PureData baud rate
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.setBrightness(100); // default brightness
  //fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
}


void loop() {
  if (Serial.available() > 0) {
    char trigger = Serial.read();

    if (trigger == 'D' && Serial.available() > 0) brightness = Serial.read();
    else if (trigger == 'E' && Serial.available() > 0) speed = Serial.read();
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

void clear(){
  FastLED.clear();
}

void trigger_state(char state){
  switch(state){
      case 'A': // 65 
        BounceAnimation(brightness); break;
      case 'B': // 66
        red(brightness); break;
      case 'C': // 67
        idle(); break;
      default: clear(); break;
  }
}

void BounceAnimation(int brightness) {
  FastLED.clear();
  for (int i = 0; i <= NUM_LEDS; i++) {
    if (current_state != 'A') return;
    leds[i] = CHSV(initial_colour, initial_saturation, brightness);
    FastLED.show();
    //delay(200);
    leds[i] = CRGB::Black;
  }

  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    if (current_state != 'A') return;
    leds[i] = CHSV(final_colour, initial_saturation, brightness);
    FastLED.show();
    //delay(200);
    leds[i] = CRGB::Black;

    // colour variation, speed, length for the bounce?
    // physics for spacing/speed? 
    // fix bounce anim 
    // look into millis
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




