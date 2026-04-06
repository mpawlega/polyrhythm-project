
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <stdarg.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include "user_interface.h"
#include <ESP8266TimerInterrupt.h>

// Debug LED
#define LED_PIN   LED_BUILTIN
#define LED_OFF   HIGH
#define LED_ON    LOW

// Select a Timer Clock
#define USING_TIM_DIV1                true           // for shortest and most accurate timer
#define USING_TIM_DIV16               false          // for medium time and medium accurate timer
#define USING_TIM_DIV256              false          // for longest timer but least accurate. Default

// Init ESP8266 only and only Timer 1
ESP8266Timer ITimer;

// compile timestamp
const char* versionCode = __DATE__ " " __TIME__;
#pragma message "\nBuilding at: " __DATE__ " " __TIME__

// jitter & drift storage stuff
#define DRIFT_SAMPLES 5000   // 5000 samples @ 1Hz = 83.3 minutes so should include the rollover @ ~71.58min
uint16_t sampleIndex = 0;
uint32_t drift[DRIFT_SAMPLES];
int      lastDriftSave = 0;
uint32_t measuredDrift = 0;

// LED strip definitions
#define NUM_LEDS 192
#define DATA_PIN D1
#define DATA_PIN_SIDES D2
#define DEBUG_PIN D7
#define DEFAULT_STRIP_PERIOD 7000000   // in microseconds
#define FAST_LED_BLOCKING_TIME 14000 // in us, measured on a 'scope and rounded up

// strip timing setup
uint32_t epochStart = 0;
bool updateLEDs = false;
unsigned int  stripDelay = DEFAULT_STRIP_PERIOD / 2 / (NUM_LEDS-1);  // in us ; "-1" because of the fencepost problem
unsigned int  stripPeriod = DEFAULT_STRIP_PERIOD;
bool runShow = false;
unsigned int  measuredPeriod = 0;
unsigned int  numPeriods = 0;
unsigned long last_LEDupdate = 0;
unsigned long lastPeriodStart = 0;
unsigned long nextLEDupdate = 0;
unsigned int missedSteps = 0;
unsigned int totalMissedSteps = 0;
int32_t stepDelta = 0;

// LED update ISR
void IRAM_ATTR updateStripISR() {
  updateLEDs = true;
}

// The "ball" is conceived as being in a 3x3 matrix, or a pixel with a 1-pixel BALL_HALO.
// To avoid having to treat the end-points 
// as edge cases when the ball has just reversed but we still have a halo, we can 
// define the LED array as being NUM_LEDS + 2*BALL_HALO long, and just pass 
// to fastled the NUM_LEDS elements from the middle of the array such that the ball centre 
// goes right to the edges.
// - ballPosition starts at BALL_HALO index (because array is zero-indexed)
// - when ballPosition reaches BALL_HALO or BALL_HALO+NUM_LEDS-1, turn around
// - pass &leds[BALL_HALO] to addleds() instead of passing leds

// ball parameters
#define BALL_HALO 1
#define DEFAULT_BALL_HUE 213
int ballPosition = BALL_HALO+NUM_LEDS-1;      // starts at the bottom of the strip
int ballDirection = -1;
int ballHue = DEFAULT_BALL_HUE;
bool flashGradient = true;

// create the RGB led struct arrays with BALL_HALO elements on ends to help with bouncing
// valid array indices go from 0 to NUM_LEDS+2*BALL_HALO-1
CRGB leds[NUM_LEDS+2*BALL_HALO];
CRGB ledsSides[NUM_LEDS+2*BALL_HALO];

// Call to redraw the ball at ball_position
void drawBall(int pos) {

  // if somehow we get passed a value that is outside the range
  // constrain it to avoid array indexing over- or underflow
  constrain(pos,BALL_HALO,BALL_HALO+NUM_LEDS-1);

  for (int led=pos+BALL_HALO; led>=pos-BALL_HALO; led--) {
    leds[led] = CHSV(213,255,255);
    ledsSides[led] = CHSV(213,255,255);
  }

}

// --- MAC to IP lookup table ---
struct MacIpPair {
  const char* mac;
  IPAddress ip;
};

MacIpPair macIpTable[] = {
  {"84:F3:EB:10:32:B6", IPAddress(192, 168, 0, 201)}, 
  {"8C:AA:B5:08:DF:0C", IPAddress(192, 168, 0, 202)}, 
  {"84:F3:EB:6C:31:70", IPAddress(192, 168, 0, 203)},
  {"40:22:D8:8E:A2:C4", IPAddress(192, 168, 0, 204)},
  {"EC:FA:BC:CD:F3:48", IPAddress(192, 168, 0, 205)},
  {"84:F3:EB:10:30:C3", IPAddress(192, 168, 0, 206)},
  {"84:F3:EB:AC:DD:BA", IPAddress(192, 168, 0, 207)},
  {"50:02:91:EE:59:DC", IPAddress(192, 168, 0, 208)},
  {"08:F9:E0:5C:F3:81", IPAddress(192, 168, 0, 209)},
  {"D8:BF:C0:00:E2:C4", IPAddress(192, 168, 0, 210)},
  {"CC:50:E3:F3:E1:D5", IPAddress(192, 168, 0, 211)},
  {"BC:DD:C2:5A:4E:6C", IPAddress(192, 168, 0, 212)},
  {"D8:BF:C0:0F:97:38", IPAddress(192, 168, 0, 213)}
};
const int numEntries = sizeof(macIpTable) / sizeof(macIpTable[0]);

// --- my IP default settings in case I'm not in the table ---
IPAddress myIP(192, 168, 0, 250); // fallback
String myIPstring = "192.168.0.250";

// --- LAN settings ---
IPAddress gatewayIP(192, 168, 0, 1);
IPAddress subnetIP(255, 255, 255, 0);
IPAddress MAXhostIP(192,168,0,200);
const char* apSSID = "MargaretAve";
const char* apPassword = "robmaudamelinesimonluc";

// --- LAN Connection state tracking ---
bool isConnected = false;

// UDP setup stuff
WiFiUDP UDP;
unsigned int listenPort = 8888;
unsigned int sendPort = 8887;
char incomingPacket[255];
int last_UDPheartbeat = 0;

// initialize OSC message addresses/headers
OSCMessage debugMsg("/debug");
OSCMessage statusMsg("/status");
OSCMessage dataMsg("/data");

template<typename T>
void sendUDPMessage(OSCMessage* msg_ptr, IPAddress receiver_IP, T first_msg) {

  // this function gets called when the last chunk is sent to sendUDPMessage

  // add the message
  (*msg_ptr).add(first_msg);

  // send the message
  UDP.beginPacket(receiver_IP, sendPort);
  (*msg_ptr).send(UDP);
  delay(100);     // delay a bit in case of consecutive messages

  (*msg_ptr).empty();
  UDP.endPacket();
}

template<typename T,typename... Args>
void sendUDPMessage(OSCMessage* msg_ptr, IPAddress receiver_IP, T first_msg, Args... rest) {

  // this gets called when more than 1 message chunk is sent to sendUDPMessage

  // add the first message, then call sendUDPMessage again with the remaining list
  (*msg_ptr).add(first_msg);
  sendUDPMessage(msg_ptr, receiver_IP, rest...);
}

void endShow() {
  runShow = false;
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Stopping Show | Periods", numPeriods, "Missed Steps", totalMissedSteps);
}

void setup() {

  // lock the CPU to 160MHz to reduce relative desync between D1s
  // due to clock descaling on one of them
  system_update_cpu_freq(160);

  // set up outputs
  pinMode(LED_PIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);     // D7
  pinMode(DATA_PIN, OUTPUT);
  pinMode(DATA_PIN_SIDES, OUTPUT);

  digitalWrite(DATA_PIN, LOW); // off
  digitalWrite(DATA_PIN_SIDES, LOW); // off
  digitalWrite(LED_PIN, LED_ON);    // turn on Debug LED during setup

  // Serial debugging start
  Serial.begin(115200);
  delay(1000);
  Serial.println("Serial up");

  // set up LED strips
  // pass a pointer to the BALL_HALO element, rather than the entire array
  // because of the "bounce halo" (e.g., if the halo is 1 LED then we pass the pointer to the second LED
  // but because of zero-indexing, that is the BALL_HALO LED)
  FastLED.addLeds<WS2812, DATA_PIN>(&leds[BALL_HALO], NUM_LEDS);
  FastLED.addLeds<WS2812, DATA_PIN_SIDES>(&ledsSides[BALL_HALO], NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // attach timer interrupts
  ITimer.attachInterruptInterval(stripDelay, updateStripISR);
  
  // get MAC address
  String mac = WiFi.macAddress();
  mac.toUpperCase();
  Serial.print("Detected MAC: ");
  Serial.println(mac);

  // --- Lookup MAC address in table ---
  bool found = false;
  for (int i = 0; i < numEntries; i++) {
    if (mac.equalsIgnoreCase(macIpTable[i].mac)) {
      myIP = macIpTable[i].ip;
      myIPstring = myIP.toString();
      found = true;
      break;
    }
  }
  Serial.print("Assigned IP: ");
  Serial.println(myIP);

  // --- Connect to wifi ---
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.config(myIP, gatewayIP, subnetIP);

  Serial.print("Looking for AP \""); Serial.print(apSSID); Serial.println("\"...");
  bool foundAP = false;
  while (!foundAP) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == apSSID) {
        foundAP = true;
        break;
      }
    }
    if (!foundAP) {
      Serial.print(".");
    }
  }

  Serial.println("Found AP, attempting to connect...");
  WiFi.begin(apSSID, apPassword);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("Assigned IP: ");
  Serial.println(WiFi.localIP());

  if (WiFi.status() == WL_CONNECTED) {
    isConnected = true;

    // Start listening for UDP from MAX
    UDP.begin(listenPort);
    Serial.print("IP ");
    Serial.print(WiFi.localIP());
    Serial.print(" listening on UDP port ");
    Serial.println(listenPort);

  } else {
    Serial.println();
    Serial.println("Failed to connect to AP after several attempts.");
  }

  
  // Set up Arduino OTA
  delay(500); // give the TCP stack a moment to settle
  Serial.println("Starting OTA...");
  String host = "D1mini_" + String(WiFi.localIP()[3]);
  ArduinoOTA.setHostname(host.c_str());
  ArduinoOTA.begin();
  Serial.println("OTA setup complete");

  // Note sendUDPMessage can't handle String types directly; append .c_str() to send a *char
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Mac address", mac.c_str());
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Connected to", apSSID);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Listening for UDP on", listenPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Sending UDP on", sendPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Timestep:", stripDelay, "(us)");
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Finished setup.  Version code", versionCode);

  digitalWrite(LED_PIN, LED_OFF); // turn off LED

}


// --- Receive UDP packets and parse them ---
void handleUDP() {

  int packetSize = UDP.parsePacket();

  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;
    int value = atoi(&incomingPacket[1]); // convert the rest to integer

    switch (incomingPacket[0]) {

      case 'Z':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Latest Show Stats: Periods", numPeriods, "Missed Steps", totalMissedSteps);
      break;

      case 'J':
      for (int i=0;i<sampleIndex;i++) {
        // sendUDPMessage(&dataMsg, MAXhostIP, myIPstring.c_str(), jitter[i]);
      }
      break;

      case 'G':
        flashGradient = !flashGradient;
      break;

      case 'C':
        for (int i=0;i<sampleIndex;i++) {
          sendUDPMessage(&dataMsg, MAXhostIP, myIPstring.c_str(), drift[i]);
        }
      break;

      case 'P':
        stripPeriod = value;
        stripDelay = stripPeriod / 2 / (NUM_LEDS-1);
        ITimer.setInterval(stripDelay, updateStripISR);
        sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Timestep to", stripDelay, "Period (us)", stripPeriod);
      break;

      // Anything that needs to be done once at start or stop, do it here
      case 'S':
        runShow = value;
        if (runShow) {
          sampleIndex = 0;
          numPeriods = 0;
          missedSteps = 0;
          totalMissedSteps = 0;
          lastDriftSave = micros(); //0;
          measuredDrift = 0;
          epochStart = micros();
          sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Starting Show | Position", ballPosition, "Direction", ballDirection, "Period", stripPeriod);
          nextLEDupdate = micros()+stripDelay;
        } else {
          endShow();
        }
      break;

      case 'V':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Version Code:", versionCode);
      break;

      case 'L':
        // the strip is reversed compared to the MAX sliders
        // both are zero-indexed but 0 is at the top of the strip and bottom of the MAX slider
        // we also have the BALL_HALO to deal with, so...
        //
        // the "top" of the strip is NUM_LEDS-1 in MAX and BALL_HALO here while
        // the "bottom" of the strip is 0 in MAX and BALL_HALO+NUM_LEDS-1 here
        ballPosition = (NUM_LEDS+BALL_HALO-1) - value;

        // The default ball direction for most starting points should be "down" on the strip which is +1
        // the exception is if the starting point is BALL_HALO+NUM_LEDS-1 which is the bottom of the strip
        // setting ballDirection=1 in that case will result in a little hiccup but the constrain call in 
        // drawBall will avoid an array indexing issue.  This should only happen if the position happens
        // to be updated right at the point where the ball hits that pixel, so I'm not going to make an 
        // exception.  If we start to see the ball "sticking" to the bottom momentarily before bouncing
        // then we can come back and make an exception
        ballDirection = (ballPosition == BALL_HALO+NUM_LEDS-1) ? -1 : 1;
        FastLED.clear();
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Setting Position:", ballPosition);
      break;

      default:
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Received Unknown Message");
      break;

    }
  }
}

void loop() {
  
  handleUDP();              // Check for UDP packets and parse them

  if (runShow) {

    // // disconnect wifi if needed
    // if (WiFi.status() == WL_CONNECTED) {
    //   WiFi.disconnect(true);  // disconnect and erase saved credentials
    //   WiFi.mode(WIFI_OFF);   // turn WiFi off to save power
    // }

    // every 1000ms, record micros since epochstart to assess clock drift over time
    // if ((millis()-lastDriftSave)>1000) {    // every second for 5000 seconds
    //   lastDriftSave = millis();
    //   measuredDrift = (micros()-epochStart);
    //   drift[sampleIndex++] = measuredDrift;
    // }
    //
    // if (sampleIndex >= DRIFT_SAMPLES) {
    //   sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Drift matrix full:", sampleIndex);
    //   endShow();
    // }

    // calculate the delta past the next LED update step (which is incremented from epochStart)
    stepDelta = (int32_t)(micros()-nextLEDupdate);
    missedSteps = 0;

    // check if we need to update the LEDs (one check using polling, one using ISR)
    if ( stepDelta >= 0 ) {         // if we passed the nextLEDupdate time
    // if (updateLEDs) {                  // if ISR has triggered flag set

      // clear them first (this doesn't display until FastLED.show() is called)
      FastLED.clear();

      // increment the epoch-based counter by the fixed timestep
      // note: nextLEDupdate is incremented from epochStart by stripDelay each time the strips are updated,
      // meaning that jitter in arriving to this point is absorbed as long as it isn't longer than the blocking
      // calls for FastLED.show() and other background blocking tasks (wifi, etc)
      nextLEDupdate += stripDelay;

      // use this drift measurement if we want to compare to the stripDelay at each cycle, instead of over a longer
      // time (see above for measurement since epochStart every 1000ms)      
      // measuredDrift = micros()-lastDriftSave;
      // lastDriftSave = micros();
      // drift[sampleIndex++] = measuredDrift;
      //
      // if (sampleIndex >= DRIFT_SAMPLES) {
      //   sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Drift matrix full:", sampleIndex);
      //   endShow();
      // }

      // check if we've actually missed >0 LED update steps; if this happens it's likely because of blocking activity 
      // from Wifi, or stripDelay being smaller than the FAST_LED_BLOCKING_TIME. It will result in some jumping, but 
      // at speeds that fast, it's not so visible so it's a good solution!
      while (micros()>nextLEDupdate) {    // essentially: are we already passed the NEW nextLEDupdate time?
        nextLEDupdate += stripDelay;
        missedSteps += 1;
      }
      totalMissedSteps += missedSteps;

      // if missedSteps >= 1 then we need to increment the ball by more than one position
      // but we also have to take into account direction changes
      for (int i=missedSteps+1; i>0; i--) {
        
        // update ball position
        ballPosition += ballDirection;

        // check limits and bounce if needed
        if (ballPosition <= BALL_HALO || ballPosition >= NUM_LEDS - 1 + BALL_HALO) {
          constrain(ballDirection,BALL_HALO,NUM_LEDS-1+BALL_HALO);
          ballDirection = -ballDirection;  // reverse direction

          if (ballDirection == -1) {          // if we're at 191 and just starting up again
            if (flashGradient) { fill_gradient(&leds[BALL_HALO], 0, CHSV(0,0,0), NUM_LEDS-1, CHSV(0,0,128)); }
            numPeriods+=1;
          } else { // if we're at 0 and starting down
            if (flashGradient) { fill_gradient(&leds[BALL_HALO], 0, CHSV(0,0,128), NUM_LEDS-1, CHSV(0,0,0));}
          }
        }
      }
      
      // draw the ball
      drawBall(ballPosition);

      // update LED strips
      digitalWrite(DEBUG_PIN,HIGH);
      FastLED.show();
      digitalWrite(DEBUG_PIN,LOW);

      // clear the update flag if using ISR
      // updateLEDs = false;
      
    } // end of if(updateLEDs)

  } else {    // if show not running
    
    // reconnect wifi if we disconnected it during the show
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(apSSID, apPassword);  
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
      }
    }

    // Check for OTA; don't need to do this if show is running
    ArduinoOTA.handle();      // check for OTA programming flag

    // Send UDP heartbeat -- NOTE THIS CAUSES STUTTER IN THE DISPLAY SO DON'T CALL DURING SHOW
    // (i.e., don't put it directly in the UDP handler routine, which is called during the show)
    if ((millis()-last_UDPheartbeat) > 5000) {
      last_UDPheartbeat = millis();
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "UDP Heartbeat");
      digitalWrite(LED_PIN, LED_ON);
      delay(500);
      digitalWrite(LED_PIN, LED_OFF);
    }

  }

}
