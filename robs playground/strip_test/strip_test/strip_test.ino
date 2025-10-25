#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <stdarg.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

// LED strip definitions
#define NUM_LEDS 192
#define DATA_PIN D1
#define DATA_PIN_MIDDLE D2
#define DEFAULT_STRIP_PERIOD 2   // in seconds


// strip timing setup
Ticker stripTicker;
bool updateLEDs = false;
int  stripPeriod = DEFAULT_STRIP_PERIOD * 1000 / 2 / NUM_LEDS;  // in ms

// The "ball" is conceived as having a position (centre) and "halo" that is BALL_HALO
// pixels. To avoid having to treat the end-points as edge cases, we can define
// the LED array as being NUM_LEDS + 2*BALL_HALO long, and just pass to fastled
// the NUM_LEDS elements from the middle of the array such that the ball centre 
// goes right to the edges.
// - ballPosition starts at BALL_HALO index
// - when ballPosition reaches BALL_HALO or BALL_HALO+NUM_LEDS, turn around
// - pass &leds[BALL_HALO] to addleds() instead of passing leds

// ball size parameters
#define BALL_HALO 3
int ballPosition = BALL_HALO;
int ballDirection = 1;

// create the led struct arrays with BALL_HALO elements on ends to help with bouncing
CRGB leds[NUM_LEDS+2*BALL_HALO];
CRGB ledsMiddle[NUM_LEDS+2*BALL_HALO];

int last_millis = 0;
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

// Called every PERIOD/2/NUM_LEDS
void updateStripISR() {
  updateLEDs = true;
}

// Call to redraw the ball at ball_position
void drawBall(int pos) {

  // shift the ball one in ballDirection
  leds[pos-ballDirection*(BALL_HALO+1)] = CHSV(0,0,0);
  leds[pos-ballDirection] = CHSV(0,0,128);
  leds[pos] = CHSV(0,0,255);
  leds[pos+ballDirection*(BALL_HALO)] = CHSV(0,0,128);

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
  digitalWrite(LED_PIN, HIGH); // off

  delay(1000);
  Serial.println("Serial up");

// set up LED strips
  // pass a pointer to the 2nd element, rather than the entire array
  // because of the "bounce halo"
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(&leds[BALL_HALO], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN_MIDDLE>(&ledsMiddle[BALL_HALO], NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  // attach timer interrupts
  stripTicker.attach_ms(stripPeriod, updateStripISR);
  blinkTicker.attach_ms(FLASH_INTERVAL, toggleLedISR);
  cycleTicker.attach_ms(CYCLE_PERIOD, startBlinkCycleISR);
  setBlinkCode(2);      // 2 flashes

  Serial.print("Set strip period to (s) ");
  Serial.println(stripPeriod);
  
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
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Finished setup.");
  
}

void handleUDP() {
  
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
    // if (incomingPacket[0] == 'F') {
      
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Flashes to", value);

      setBlinkCode(value);
      break;

      case 'P':
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Period to", value);
      stripTicker.attach_ms(value * 500 / NUM_LEDS, updateStripISR);
      break;

      default:
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Unexpected:", value);
      break;

    }
  }

}

void loop() {
  
  ArduinoOTA.handle();      // check for OTA programming flag
  handleUDP();              // Check for UDP packets and parse them

  // check if an LED update is needed
  if (updateLEDs) {
  // if ((millis()-last_millis)>=stripPeriod) {
    last_millis = millis();

    // update ball position
    ballPosition += ballDirection;

    // draw the ball
    drawBall(ballPosition);

    // check limits and bounce if needed
    if(ballPosition == BALL_HALO || ballPosition == BALL_HALO + NUM_LEDS - 1) {
      ballDirection = -ballDirection;  // reverse direction
    }
    
    FastLED.show();

    updateLEDs = false;

  }


  
}
