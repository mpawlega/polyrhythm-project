#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <stdarg.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

// compile timestamp
const char* versionCode = __DATE__ " " __TIME__;
#pragma message "\nBuilding at: " __DATE__ " " __TIME__

// LED strip definitions
#define NUM_LEDS 192
#define DATA_PIN D1
#define DATA_PIN_SIDES D2
#define DEBUG_PIN D7
#define DEFAULT_STRIP_PERIOD 7   // in seconds

// strip timing setup
Ticker stripTicker;
int  stripDelay = DEFAULT_STRIP_PERIOD * 1000000 / 2 / (NUM_LEDS-1);  // in us ; "-1" because of the fencepost problem
bool runShow = false;
int  measuredPeriod = 0;
int  numPeriods = 0;
unsigned long last_LEDupdate = 0;
unsigned long lastPeriodStart = 0;


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
#define MAX_TAIL_LENGTH 10
#define DEFAULT_BALL_HUE 213
int tailLength = 0;
int ballPosition = BALL_HALO+NUM_LEDS-1;      // starts at the bottom of the strip
int ballDirection = -1;
int ballHue = DEFAULT_BALL_HUE;

// create the RGB led struct arrays with BALL_HALO elements on ends to help with bouncing
// valid array indices go from 0 to NUM_LEDS+2*BALL_HALO-1
CRGB leds[NUM_LEDS+2*BALL_HALO];
CRGB ledsSides[NUM_LEDS+2*BALL_HALO];
// CRGB leds[NUM_LEDS+2*(BALL_HALO+MAX_TAIL_LENGTH)];
// CRGB ledsSides[NUM_LEDS+2*(BALL_HALO+MAX_TAIL_LENGTH)];

// construct some HSV arrays to hold the animation pieces so it's easy to adjust hue and value
// CHSV ball[2][3] = {
//   { CHSV(DEFAULT_BALL_HUE,255,0), CHSV(DEFAULT_BALL_HUE,255,128), CHSV(DEFAULT_BALL_HUE,255,0) },
//   { CHSV(DEFAULT_BALL_HUE,255,128), CHSV(DEFAULT_BALL_HUE,255,255), CHSV(DEFAULT_BALL_HUE,255,128) }
// };

// int delay_time = 50;
// int bounce_delay_time = 75;
// int trail_length = 15;
// int bounce_length = 30;
// int brightness[15] = {255, 238, 221, 204, 187, 170, 153, 
//                       136, 119, 102,  85,  68,  51,  38, 24};

// Status LED blinking interrupt stuff
const int LED_PIN = LED_BUILTIN;   // Active-LOW on Wemos D1 mini/lite
Ticker blinkTicker;
Ticker cycleTicker;

volatile int blinkCode = 0;        // 0–20, sets number of flashes per cycle (0=off, 20=on)
volatile int currentFlash = 0;
volatile bool ledState = false;

// LED timing constants
const float FLASH_INTERVAL = 50; // milli-seconds per half-cycle → 10 Hz full blink
const float CYCLE_PERIOD = 2000;    // milli-seconds total per code cycle

// LED ISRs - setBlinkCode(N=2..20) flashes N times in 2s at 10Hz (0=off, 20=on)
void toggleLedISR() {
  // If code is 0 → LED off
  if (blinkCode == 0) {
    digitalWrite(LED_PIN, HIGH); // off (active-low)
    return;
  }

  // If code is 20 → LED on solid
  if (blinkCode == 20) {
    digitalWrite(LED_PIN, LOW);  // on (active-low)
    return;
  }

  // Otherwise, blink "blinkCode" times at 10 Hz, then stay off for rest of 2s
  if (currentFlash < blinkCode * 2) { // each flash = on + off
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    currentFlash++;
  } else {
    // End of flashing sequence → LED off, wait until next cycle
    digitalWrite(LED_PIN, HIGH);
  }
}

// Called once per cycle to reset flash counter
void startBlinkCycleISR() {
  currentFlash = 0;
}

// Build ball matrix
void makeBall(int hue) {

  // for (int row = 0; row < 2; row++) {
  //   for (int col = 0; col < 3; col++) {
  //       ball[row][col].h = hue;   // set hue for every pixel
  //   }
  // }


}

// Call to redraw the ball at ball_position
void drawBall(int pos) {

  // need to adjust pixels from one ahead of pos, to 1+tailLength+1 behind pos
  // switch (ballDirection) {
  //   case 1:                   // going up
  //     for (int i=0; i<MAX_TAIL_LENGTH; i++) {
  //       ledsSides[pos-(BALL_HALO+1+i)] = tail[1][i];
  //       leds[pos-(BALL_HALO+1+i)] = tail[2][i];
  //     }
  //     for (int i=0; i<(1+2*BALL_HALO); i++) {
  //       ledsSides[pos-(BALL_HALO-i)] = ball[1][i];
  //       leds[pos-(BALL_HALO-i)] = ball[2][i];
  //     }
        
  //   break;

  //   case -1:                  // going down
  //     for (int i=0; i<MAX_TAIL_LENGTH; i++) {
  //       ledsSides[pos+(BALL_HALO+1+i)] = tail[1][i];
  //       leds[pos+(BALL_HALO+1+i)] = tail[2][i];
  //     }
  //     for (int i=0; i<(1+2*BALL_HALO); i++) {
  //       ledsSides[pos+(BALL_HALO-i)] = ball[1][i];
  //       leds[pos+(BALL_HALO-i)] = ball[2][i];
  //     }

  //   break;
  // }

  // if somehow we get passed a value that is outside the range
  // constrain it to avoid array indexing over- or underflow
  constrain(pos,BALL_HALO,BALL_HALO+NUM_LEDS-1);

  // // shift the centre strip one in ballDirection
  leds[pos-ballDirection*(BALL_HALO+1)] = CHSV(0,0,0);
  leds[pos-ballDirection] = CHSV(213,255,255);
  leds[pos] = CHSV(213,255,255);
  leds[pos+ballDirection*(BALL_HALO)] = CHSV(213,255,255);

  // // side strips = copy centre strip at half-brightness (value)
  ledsSides[pos-ballDirection*(BALL_HALO+1)] = CHSV(0,0,0);
  ledsSides[pos-ballDirection] = CHSV(213,255,255);
  ledsSides[pos] = CHSV(213,255,255);
  ledsSides[pos+ballDirection*(BALL_HALO)] = CHSV(213,255,255);

}

// Call this from main code to change the blink code dynamically
void setBlinkCode(int code) {
  blinkCode = constrain(code, 0, 20);
}

// --- MAC to IP lookup table ---
struct MacIpPair {
  const char* mac;
  IPAddress ip;
};

MacIpPair macIpTable[] = {
  {"84:F3:EB:10:32:B6", IPAddress(192, 168, 0, 201)}, // Client 1
  // {"84:F3:EB:10:32:B6", IPAddress(192, 168, 3, 1)}, // The AP board
  {"50:02:91:EE:59:DC", IPAddress(192, 168, 0, 202)}, // Client 2
  // and so on...
};
const int numEntries = sizeof(macIpTable) / sizeof(macIpTable[0]);

// --- my IP default settings ---
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

// initialize OSC message addresses/headers
OSCMessage debugMsg("/debug");
OSCMessage statusMsg("/status");

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

void setup() {

  // Serial debugging start
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(DATA_PIN_SIDES, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // off
  digitalWrite(DATA_PIN, LOW); // off
  digitalWrite(DATA_PIN_SIDES, LOW); // off
  
  delay(1000);
  Serial.println("Serial up");

// set up LED strips
  // pass a pointer to the 2nd element, rather than the entire array
  // because of the "bounce halo"
  FastLED.addLeds<WS2811, DATA_PIN>(&leds[BALL_HALO], NUM_LEDS);
  FastLED.addLeds<WS2811, DATA_PIN_SIDES>(&ledsSides[BALL_HALO], NUM_LEDS);

  // FastLED.addLeds<NEOPIXEL, DATA_PIN>(&leds[BALL_HALO+MAX_TAIL_LENGTH], NUM_LEDS);
  // FastLED.addLeds<NEOPIXEL, DATA_PIN_SIDES>(&ledsSides[BALL_HALO+MAX_TAIL_LENGTH], NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // make ball and tail
  // makeBall(ballHue);
  // makeTail(tailLength);

  // attach timer interrupts
  blinkTicker.attach_ms(FLASH_INTERVAL, toggleLedISR);
  cycleTicker.attach_ms(CYCLE_PERIOD, startBlinkCycleISR);
  setBlinkCode(2);      // 2 flashes

  Serial.print("Set strip period to (s) ");
  Serial.println(stripDelay);
  
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
  WiFi.config(myIP, gatewayIP, subnetIP);

  Serial.print("Looking for AP \""); Serial.print(apSSID); Serial.println("\"...");
  bool foundAP = false;
  while (!foundAP) {
    setBlinkCode(3); // blink 3 times while searching
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
  // WiFi.setSleepMode(WIFI_NONE_SLEEP);
  setBlinkCode(5);    // blink 5 times while connecting to SSID

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

    setBlinkCode(20);    // turn LED on solid

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

  // ArduinoOTA.onStart([]() {
  //   Serial.println("OTA start: pausing animation");
  //   runShow = false;    // automatically pause the show
  // });

  // ArduinoOTA.onEnd([]() {
  //   Serial.println("\nOTA end: resuming animation");
  //   runShow = true;     // resume animation after OTA
  // });

  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //   Serial.printf("OTA progress: %u%%\r", (progress * 100) / total);
  // });

  // ArduinoOTA.onError([](ota_error_t error) {
  //   Serial.printf("OTA error[%u]: ", error);
  //   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //   else if (error == OTA_END_ERROR) Serial.println("End Failed");
  //   runShow = true; // make sure show resumes even on error
  // });

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
  sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Timestep:", stripDelay, "(us)");
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Finished setup.  Version code", versionCode);

  
}

int last_UDPheartbeat = 0;

void handleUDP() {
  
  // // Send UDP heartbeat -- NOTE THIS CAUSES STUTTER IN THE DISPLAY SO DON'T CALL DURING SHOW
  if (!runShow) {
    if ((millis()-last_UDPheartbeat) > 5000) {
      last_UDPheartbeat = millis();
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "UDP Heartbeat");
    }
  }


  // --- Receive UDP packets and parse them ---
  int packetSize = UDP.parsePacket();

  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;
    int value = atoi(&incomingPacket[1]); // convert the rest to integer

    // Serial.print("I got one! ");
    // Serial.println(incomingPacket);

    switch (incomingPacket[0]) {
      case 'F': 
      setBlinkCode(value);
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Flashes to", value);
      break;

      case 'P':
      stripDelay = value * 1000000 / 2 / (NUM_LEDS-1);
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Timestep to", stripDelay);
      break;

      case 'S':
      runShow = value;
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Run Show:", runShow, "Position", ballPosition, "Direction", ballDirection);
      break;

      case 'V':
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Version Code:", versionCode);
      break;

      case 'T':
      stripDelay = value;
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Strip Timestep:", stripDelay);
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
  
  ArduinoOTA.handle();      // check for OTA programming flag
  handleUDP();              // Check for UDP packets and parse them

  if (runShow) {
    // check if an LED update is needed
    if ((micros()-last_LEDupdate)>=stripDelay) {
      last_LEDupdate = micros();

      // update ball position
      ballPosition += ballDirection;

      // draw the ball
      drawBall(ballPosition);

      // check limits and bounce if needed
      if(ballPosition <= BALL_HALO || ballPosition >= BALL_HALO + NUM_LEDS - 1) {
        ballDirection = -ballDirection;  // reverse direction

        if (ballDirection == -1) {          // if we're at 191 and just starting up again
          numPeriods+=1;
          measuredPeriod = micros() - lastPeriodStart;
          lastPeriodStart = micros();
          // sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), numPeriods, measuredPeriod);
        }
      }
      
      digitalWrite(DEBUG_PIN,HIGH);
      FastLED.show();
      digitalWrite(DEBUG_PIN,LOW);
    }
  }

}
