// --- Begin adapted sketch: original comments preserved, diagnostic additions marked [DIAG] ---

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <stdarg.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include "user_interface.h"

// Debug LED
#define LED_PIN   LED_BUILTIN
#define LED_OFF   HIGH
#define LED_ON    LOW

// compile timestamp
const char* versionCode = __DATE__ " " __TIME__;
#pragma message "\nBuilding at: " __DATE__ " " __TIME__

// sampling timing stuff
#define DRIFT_SAMPLES 3600   // 3600 samples @ 1Hz = 1 hour (use uint64 timestamps)
#define SAMPLE_INTERVAL_US 4000000ULL  // in us; the ULL suffix specifies 64-bit (unsigned long long)
uint64_t samples64[DRIFT_SAMPLES];
// uint64_t nextSampleTime64 = 0;
uint16_t sampleIndex = 0;
bool     sampleTiming = false;

// LED strip definitions
#define NUM_LEDS 192
#define DATA_PIN D1
#define DATA_PIN_SIDES D2
#define DEBUG_PIN D7
#define DEFAULT_STRIP_PERIOD 7000000.0   // in microseconds
#define STRIP_TICKS (2*(NUM_LEDS-1))

// strip timing stuff
uint64_t epochStart64  = 0;
uint64_t nextLEDupdate64 = 0;
// uint32_t frameIndex = 0;      // this allows > 11 Million periods of 382 LEDs so 32 bits is fine
uint32_t stripDelay = DEFAULT_STRIP_PERIOD / STRIP_TICKS;  // in us ; "-1" because of the fencepost problem
// uint32_t stripPeriod = DEFAULT_STRIP_PERIOD;
float stripPeriod = DEFAULT_STRIP_PERIOD;   // fractional period in us from MAX
float idealDelay = DEFAULT_STRIP_PERIOD / STRIP_TICKS;  // fractional, in us 
uint32_t numPeriods = 0;
uint32_t missedFrames = 0;
uint32_t totalMissedFrames = 0;
bool     runShow = false;
bool     sendHeartbeat = false;
uint32_t delayPattern[STRIP_TICKS];    // there are NUM_LEDS-1 steps between NUM_LEDS leds
uint8_t  delayPatternIndex = 0;
int      gradientLength = 24;




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
#define BALL_HALO 0
#define DEFAULT_BALL_HUE 213
#define START_BALL_POSITION BALL_HALO+NUM_LEDS-1  // starts at the bottom of the strip
#define START_BALL_DIRECTION -1                   // starts heading up the strip towards index 0
int     ballDirection = START_BALL_DIRECTION;
uint8_t ballPosition = START_BALL_POSITION;      
uint8_t ballHue = DEFAULT_BALL_HUE;
bool    flashGradient = true;

// create the RGB led struct arrays with BALL_HALO elements on ends to help with bouncing
// valid array indices go from 0 to NUM_LEDS+2*BALL_HALO-1
CRGB leds[NUM_LEDS+2*BALL_HALO];
CRGB ledsSides[NUM_LEDS+2*BALL_HALO];

// Call to redraw the ball at ball_position
void drawBall(int pos) {

  // if somehow we get passed a value that is outside the range
  // constrain it to avoid array indexing over- or underflow
  pos = constrain(pos,BALL_HALO,BALL_HALO+NUM_LEDS-1);

  for (int led=pos+BALL_HALO; led>=pos-BALL_HALO; led--) {
    leds[led] = CHSV(ballHue,255,255);
    ledsSides[led] = CHSV(ballHue,255,255);
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
  {"D8:BF:C0:0F:97:38", IPAddress(192, 168, 0, 213)},
  {"84:F3:EB:10:30:54", IPAddress(192, 168, 0, 214)},
  {"b4:e6:2d:55:39:93", IPAddress(192, 168, 0, 215)},
  {"8c:aa:b5:c4:f9:68", IPAddress(192, 168, 0, 216)}};
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
unsigned int debugPort = 8890;    // for python listeners; MAX uses OSC flags to filter debug messages
char incomingPacket[255];
char msg[128];
int last_UDPheartbeat = 0;

// initialize OSC message addresses/headers
OSCMessage debugMsg("/debug");
OSCMessage statusMsg("/status");
OSCMessage dataMsg("/data");
OSCMessage binaryMsg("/binary");    // in case I implement receiving binary data packets on MAX vs. using data.py

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

void sendDebugUDP(const char* msg) {
  UDP.beginPacket(MAXhostIP, debugPort);
  UDP.write((const uint8_t*)msg, strlen(msg));
  UDP.endPacket();
}

// build the pattern array for indexed delays
void buildDelayPattern(float idealDelay) {
  
  // compute the integer delays
  uint32_t lowDelay = (uint32_t)floor(idealDelay);
  uint32_t highDelay = lowDelay + 1;

  // figure out how many high vs. low we need
  float fractionHigh = ( idealDelay - (float)lowDelay );
  // uint32_t highCount = (uint32_t)( (fractionHigh * (float)STRIP_TICKS) + 0.5);
  // if (highCount > STRIP_TICKS) highCount = STRIP_TICKS;

  // fill the matrix using Bresenham Algorithm
  int highCount = 0;
  int lowCount = 0;
  float error = 0.0;
  for (int i=0; i<STRIP_TICKS; i++) {
    error += fractionHigh;
    if (error >= 1.0) {
      delayPattern[i] = highDelay;
      highCount++;
      error -= 1.0;
    } else {
      lowCount++;
      delayPattern[i] = lowDelay;
    }
  }

  sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(),
                 "Built delay array with", lowDelay, highDelay, ". Counts:", 
                 lowCount, highCount, fractionHigh, "Period:", (lowDelay*lowCount+highDelay*highCount));

}

void startShow() {
  runShow = true;
  sendHeartbeat = false;
  sampleIndex = 0;
  numPeriods = 0;
  // frameIndex = 0;
  missedFrames = 0;
  totalMissedFrames = 0;
  buildDelayPattern(idealDelay);
  delayPatternIndex = 0;
  sampleIndex = 0;
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Starting Show | Position", ballPosition, "Direction", ballDirection, "Period", stripPeriod);
  epochStart64 = micros64();
  nextLEDupdate64 = epochStart64 + (uint64_t)delayPattern[0];
}

void endShow() {
  runShow = false;
  // FastLED.clear();
  // FastLED.show();
  // ballPosition = START_BALL_POSITION;
  // ballDirection = START_BALL_DIRECTION;
  // sendHeartbeat = true;
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Stopping Show | Periods", numPeriods, "Missed Steps", totalMissedFrames);
}

// ------------------- [DIAG] helpers and UDP binary export -------------------
// [DIAG] write big-endian helpers for binary UDP packet construction
inline void write_u32_be(uint8_t *buf, uint32_t v) {
  buf[0] = (v >> 24) & 0xFF;
  buf[1] = (v >> 16) & 0xFF;
  buf[2] = (v >> 8) & 0xFF;
  buf[3] = (v) & 0xFF;
}
inline void write_u16_be(uint8_t *buf, uint16_t v) {
  buf[0] = (v >> 8) & 0xFF;
  buf[1] = (v) & 0xFF;
}
inline void write_u64_be(uint8_t *buf, uint64_t v) {
  buf[0] = (v >> 56) & 0xFF;
  buf[1] = (v >> 48) & 0xFF;
  buf[2] = (v >> 40) & 0xFF;
  buf[3] = (v >> 32) & 0xFF;
  buf[4] = (v >> 24) & 0xFF;
  buf[5] = (v >> 16) & 0xFF;
  buf[6] = (v >> 8) & 0xFF;
  buf[7] = (v) & 0xFF;
}

// [DIAG] Packet format:
// [4 bytes start_index BE][2 bytes count BE][count * 8 bytes timestamps BE]
void sendSamplesUDP(uint32_t start_idx, uint16_t count) {
  if (start_idx >= sampleIndex) return; // nothing to send
  if (start_idx + count > sampleIndex) count = sampleIndex - start_idx;


  // compute packet size and create buffer
  uint16_t header = 6;
  uint32_t payload_bytes = (uint32_t)count * 8;
  uint32_t pkt_size = header + payload_bytes;

  // snprintf(msg, sizeof(msg), "/download [%s] Constructing packet with %u bytes", myIPstring.c_str(), payload_bytes);
  // sendDebugUDP(msg);

  // Guard: if packet > 1400, reduce count (shouldn't happen if chunk size is tuned)
  if (pkt_size > 1400) {
    uint16_t newcount = (1400 - header) / 8;
    if (newcount == 0) return;
    count = newcount;
    payload_bytes = (uint32_t)count * 8;
    pkt_size = header + payload_bytes;
  }

  uint8_t *buf = (uint8_t*) malloc(pkt_size);
  if (!buf) return;

  // header: start_idx (4B BE) + count (2B BE)
  write_u32_be(buf, start_idx);
  write_u16_be(buf + 4, count);

  // snprintf(msg, sizeof(msg), "/download [%s] Writing packet to buffer", myIPstring.c_str());
  // sendDebugUDP(msg);

  // copy samples (big-endian)
  for (uint16_t i = 0; i < count; ++i) {
    uint64_t v = samples64[start_idx + i];
    write_u64_be(buf + header + (i * 8), v);
  }

  // snprintf(msg, sizeof(msg), "/download [%s] Sending Packet with size %u", myIPstring.c_str(), pkt_size);
  // sendDebugUDP(msg);

  // send raw UDP packet to MAXhostIP:sendPort
  UDP.beginPacket(MAXhostIP, sendPort);
  UDP.write(buf, pkt_size);
  UDP.endPacket();

  free(buf);
}

// [DIAG] send all recorded samples in chunks (caller can use 'A' to request)
#define UDP_SAMPLE_CHUNK 64
void sendAllSamplesInChunks() {
  uint32_t idx = 0;
  while (idx < sampleIndex) {
    sendSamplesUDP(idx, UDP_SAMPLE_CHUNK);
    idx += UDP_SAMPLE_CHUNK;
    delay(20); // small breathing room for the network stack
  }
}
// ---------------------------------------------------------------------------

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

  // set up delay pattern matrix using default period
  buildDelayPattern(idealDelay);

  // clear the LEDs
  FastLED.clear();
  FastLED.show();
  delay(100);

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

  // Note: this UDP handler is very fragile and relies on only ever receiving well-formed packets.
  // In a controlled environment with little data packet corruption it's probably fine.
  // At a minimum, unrecognized packets are responded to with a ??? message.
  // But also UDP drops packets and doesn't guarantee sequencing, so it's fragile by nature.
  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;
    int intValue = atoi(&incomingPacket[1]); // convert the rest to integer
    float floatValue = atof(&incomingPacket[1]); // convert the rest to float

    switch (incomingPacket[0]) {

      // send latest run stats
      case 'Z':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Latest Show Stats: Periods", numPeriods, "Missed Steps", totalMissedFrames);
      break;

      case 'G':
        if (intValue==0) {
          flashGradient = false;
        } else {
          flashGradient = true;
          gradientLength = intValue;
        }
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Setting gradient length", gradientLength);
      break;

      case 'B':
        sendHeartbeat = !sendHeartbeat;
      break;

      case 'H':
        ballHue = (uint8_t)intValue;   // cast as byte just in case, so this doesn't break other things if >255
      break;

      case 'C':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Clearing LED strips");
        FastLED.clear();
        FastLED.show();
        delay(100);
      break;

      // stripPeriod, idealDelay are floats
      case 'P':
        stripPeriod = floatValue;
        if (stripPeriod<=0.0) {
          sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Got bad period:", stripPeriod);
          break;
        }
        idealDelay = (stripPeriod+0.5) / (float)STRIP_TICKS;
        sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Period to", stripPeriod, "Timesteps:", idealDelay);
        buildDelayPattern(idealDelay);
      break;

      case 'S':
        if (intValue) {
          startShow();
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
        ballPosition = (NUM_LEDS+BALL_HALO-1) - intValue;

        // The default ball direction for most starting points should be "down" on the strip which is +1
        // the exception is if the starting point is BALL_HALO+NUM_LEDS-1 which is the bottom of the strip
        ballDirection = (ballPosition == BALL_HALO+NUM_LEDS-1) ? -1 : 1;
        FastLED.clear();
        drawBall(ballPosition);
        FastLED.show();
        delay(100);
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Set Position:", ballPosition);
      break;

      // ---------- [DIAG] measurement and export commands ----------
      // 'T' <1=start measurement, 0=stop measurement>
      case 'T':
        if (intValue != 0) {
          // start timestamp sampling and reset sampling array
          sampleTiming = true;
          sampleIndex = 0;
          // turn on sampling mode by scheduling the first sample and incrementing sampleIndex past 0
          uint64_t t0 = micros64();
          samples64[0] = t0; // sample index 0 stored immediately at start (optional)
          sampleIndex = 1;
          // schedule next sample after interval
          // nextSampleTime64 stored in microseconds monotonic
          // uint64_t nextSampleTime64 = t0 + SAMPLE_INTERVAL_US; // 1 Hz => 1,000,000 us; defined as ULL
          // store scheduling into a static-like variable by writing to a global-like named var
          // Because original code didn't have a nextSampleTime64 variable, we create a static here:
          // we will use the global variable 'nextLEDupdate' temporarily to hold nextSampleTime64 during measurement
          // nextLEDupdate = (unsigned long)(nextSampleTime64 & 0xFFFFFFFFUL); // low 32 bits used only for scheduling
          // send notification
          sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Measurement STARTED. Samples:", DRIFT_SAMPLES, "Interval (us)", (uint32_t)SAMPLE_INTERVAL_US);
        } else {
          // stop measurement
          sampleTiming = false;
          sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Measurement STOPPED. Recorded:", sampleIndex);
        }
      break;

      // 'R' <start_index> — send up to UDP_SAMPLE_CHUNK samples starting at start_index as binary UDP packet
      case 'R':
      {
        uint32_t start_idx = (uint32_t) intValue;
        // snprintf(msg, sizeof(msg), "/download [%s] Requesting chunk at index %u", myIPstring.c_str(), start_idx);
        // sendDebugUDP(msg);
        // [DIAG] compute count using chunk defined above
        sendSamplesUDP(start_idx, UDP_SAMPLE_CHUNK);
      }
      break;

      // 'A' — send all samples in chunks (binary)
      case 'A':
        sendAllSamplesInChunks();
      break;

      default:
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Received Unknown Message");
      break;

    }
  }
}

void loop() {
  
  handleUDP();              // Check for UDP packets and parse them

  // regardless of whether the show is running or not, if we're sampling the clock do it here
  // note: to start sampling, the UDP handler T1 code sets sampleIndex = 1 to enter this loop
  if (sampleTiming && (sampleIndex > 0 && sampleIndex < DRIFT_SAMPLES)) {
    uint64_t now = micros64();
    // uint64_t nextSampleTime64 = ((uint64_t)nextLEDupdate) & 0xFFFFFFFFULL;
    // // If the reconstructed nextSampleTime64 is less than now's low 32 bits, adjust by adding 2^32 until > now-1s
    // // Simpler: use a rolling schedule based on the first sample saved in samples64[0]
    uint64_t scheduledSample64 = samples64[0] + (uint64_t)(sampleIndex) * SAMPLE_INTERVAL_US;
    if (now >= scheduledSample64) {
      // record sample
      samples64[sampleIndex++] = now;
      // if buffer full, send notification (host can request data)
      if (sampleIndex >= DRIFT_SAMPLES) {
        // buffer full: notify host
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Measurement BUFFER FULL", "Samples", sampleIndex);
      }
      // small yield to avoid blocking UDP
      yield();
    }
    // while measuring we avoid heavy blocking actions; we still let UDP run above
    // if (sampleIndex > 0 && sampleIndex < DRIFT_SAMPLES) return;
  }

  if (runShow) {

    // // disconnect wifi if needed
    // if (WiFi.status() == WL_CONNECTED) {
    //   WiFi.disconnect(true);  // disconnect and erase saved credentials
    //   WiFi.mode(WIFI_OFF);   // turn WiFi off to save power
    // }

    // Use monotonic 64-bit time for scheduling math
    uint64_t now = micros64();
    // If we've passed the update time, do the update
    if (now >= nextLEDupdate64) {

      // count missedFrames by iterating and adding the adaptive step delays from
      // delayPattern array.  missedFrames includes the current frame.
      missedFrames = 0;

      // Safety cap so a very late system doesn't spin forever
      const uint32_t SAFETY_MAX_STEPS = 10000;

      // Advance at most SAFETY_MAX_STEPS steps to catch up
      while ((now >= nextLEDupdate64) && (missedFrames < SAFETY_MAX_STEPS)) {

        // determine current step delay from pattern
        uint32_t curDelay = delayPattern[delayPatternIndex];

        // advance nextLEDupdate by this step's delay
        nextLEDupdate64 += (uint64_t)curDelay;

        // advance pattern index and frame index
        delayPatternIndex++;
        if (delayPatternIndex >= STRIP_TICKS) {
          delayPatternIndex = 0;
          // optionally increment numPeriods when pattern wraps (if you want to count full periods)
        }

        // frameIndex++;
        missedFrames++;
      }

      // if we hit the cap, send a debug message once
      if (missedFrames >= SAFETY_MAX_STEPS) {
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Warning: catch-up cap hit", (uint32_t)missedFrames);
      }

      // Update total (note, missedFrames includes the current one, which is not really "missed")
      if (missedFrames > 0) totalMissedFrames += (missedFrames - 1); 
    
// ------- this was replaced by adaptive step code
    // // If we've passed the update time, do the update
    // if (now >= nextLEDupdate64) {

    //   // compute how many frames we might have missed using integer division
    //   uint64_t delta64 = now - nextLEDupdate64;
    //   missedFrames = (uint32_t)(delta64 / (uint64_t)stripDelay);
   
    //   // Update frameIndex and compute the *new* nextLEDupdate64 strictly from epoch,
    //   // which prevents accumulation of rounding errors that would occur if we just += stripDelay repeatedly.
    //   // We advance by missedFrames + 1 to include the current frame.
    //   frameIndex += (uint32_t)(missedFrames + 1U);
    //   nextLEDupdate64 = epochStart64 + ((uint64_t)frameIndex * (uint64_t)stripDelay);

    //   // increment the total missed frames stat
    //   totalMissedFrames += missedFrames;
// ------------

       // clear the strips first (this doesn't display until FastLED.show() is called)
      FastLED.clear();

      // if missedFrames >= 1 then we need to increment the ball by more than one position
      // but we also have to take into account direction changes, so go one step at a time
      for (int i=missedFrames; i>0; i--) {
        
        // update ball position
        ballPosition += ballDirection;

        // check limits and bounce if needed
        if (ballPosition <= BALL_HALO || ballPosition >= NUM_LEDS - 1 + BALL_HALO) {
          ballPosition = constrain(ballPosition,BALL_HALO,NUM_LEDS-1+BALL_HALO);
          ballDirection = -ballDirection;  // reverse direction

          if (ballDirection == -1) {          // if we're at 191 and just starting up again
            if (flashGradient) { fill_gradient(&leds[BALL_HALO], NUM_LEDS-1-gradientLength, CHSV(0,0,0), NUM_LEDS-1, CHSV(0,0,128)); }
            numPeriods++;
          } else { // if we're at 0 and starting down
            if (flashGradient) { fill_gradient(&leds[BALL_HALO], 0, CHSV(0,0,128), gradientLength, CHSV(0,0,0));}
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

    // // Clear the strips
    // FastLED.clear();
    // FastLED.show();

    // Send UDP heartbeat -- NOTE THIS CAUSES STUTTER IN THE DISPLAY SO DON'T CALL DURING SHOW
    // (i.e., don't put it directly in the UDP handler routine, which is called during the show)
    if (sendHeartbeat && ((millis()-last_UDPheartbeat) > 5000)) {
      last_UDPheartbeat = millis();
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "UDP Heartbeat");
      snprintf(msg, sizeof(msg), "/download [%s] Heartbeat", myIPstring.c_str());
      sendDebugUDP(msg);
      digitalWrite(LED_PIN, LED_ON);
      delay(500);
      digitalWrite(LED_PIN, LED_OFF);
    }

  }

} // end loop