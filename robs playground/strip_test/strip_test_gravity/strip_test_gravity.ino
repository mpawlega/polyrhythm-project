// Full updated D1mini sketch with epoch-driven physics integrator and mode switch
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

// ---------------------- NEW PHYSICS DEFINES ----------------------
// physics fixed-step in microseconds (tunable) — recommended 1–2 ms
#define SIM_DT 1500ULL

// gravity in LED-units per second^2 (positive = down, toward larger index)
#define GRAVITY_LED_PER_S2 4000.0f

// coefficient of restitution (1.0 = perfectly elastic)
#define RESTITUTION 1.0f

// burst flash duration (ms)
#define BURST_DURATION_MS 90

// ----------------------------------------------------------------

// sampling timing stuff
#define DRIFT_SAMPLES 3600   // 3600 samples @ 1Hz = 1 hour (use uint64 timestamps)
#define SAMPLE_INTERVAL_US 4000000ULL  // in us
uint64_t samples64[DRIFT_SAMPLES];
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
uint32_t stripDelay = (uint32_t)(DEFAULT_STRIP_PERIOD / STRIP_TICKS);  // in us
float stripPeriod = DEFAULT_STRIP_PERIOD;   // fractional period in us from MAX
float idealDelay = DEFAULT_STRIP_PERIOD / STRIP_TICKS;  // fractional, in us 
uint32_t numPeriods = 0;
uint32_t missedFrames = 0;
uint32_t totalMissedFrames = 0;
bool     runShow = false;
bool     sendHeartbeat = false;
bool     gravityMode = false;
uint32_t delayPattern[STRIP_TICKS];    // there are NUM_LEDS-1 steps between NUM_LEDS leds
uint8_t  delayPatternIndex = 0;
int      bottomGradientLength = 24;
int      topGradientLength = 24;
float    gravity = 400.0;

// physics-based integration variables
// ball position as a float for the simulation (0 to NUM_LEDS-1) and velocity (LEDs/sec)
float simBallPosition = 0.0f;
float simBallVelocity = 0.0f;

// last physics epoch and accumulator (both in microseconds)
uint64_t lastSimTime = 0;
uint64_t timeAccumulator = 0ULL;

// integer position used for rendering & manual jogs (includes BALL_HALO offset)
int ballPosition = 0;   // replaced original uint8_t for safety in arithmetic (we map back when rendering)
int ballDirection = -1; // -1 = up, +1 = down (kept for compatibility with jog + gradient code)
uint8_t ballHue = 213;

// fixed-sweep velocity (LEDs/sec) computed from stripPeriod
float v_fixed = 0.0f;

// burst/flash state triggered on bounce
uint32_t burstExpiresAtMs = 0;
bool burstAtBottom = false;  // true -> bottom burst, false -> top burst

// ----------------------------------------------------------------

// The "ball" halo and rendering arrays
#define BALL_HALO 0
#define DEFAULT_BALL_HUE 213
#define START_BALL_POSITION (BALL_HALO + NUM_LEDS - 1)  // starts at bottom
#define START_BALL_DIRECTION -1
uint8_t ballHue_u8 = DEFAULT_BALL_HUE;
bool    flashBottomGradient = true;
bool    flashTopGradient = true;

// create the RGB led struct arrays with BALL_HALO elements on ends to help with bouncing
CRGB leds[NUM_LEDS+2*BALL_HALO];
CRGB ledsSides[NUM_LEDS+2*BALL_HALO];

// forward declarations
void integratePhysics(float dt_s);
void updateVFixedFromPeriod();
void drawBallAtInteger(int integerIndex);
void moveBallInt(int direction);

// Call to redraw the ball at ball_position (ballPosition includes BALL_HALO)
void drawBallAtInteger(int posIndex) {
  int pos = posIndex; // assume caller passes already offset index into leds array
  // guard
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

// ... (your macIpTable unchanged) ...
MacIpPair macIpTable[] = {
  {"84:F3:EB:10:32:B6", IPAddress(192, 168, 2, 201)}, 
  {"8C:AA:B5:08:DF:0C", IPAddress(192, 168, 2, 202)}, 
  {"84:F3:EB:6C:31:70", IPAddress(192, 168, 2, 203)},
  {"40:22:D8:8E:A2:C4", IPAddress(192, 168, 2, 204)},
  {"EC:FA:BC:CD:F3:48", IPAddress(192, 168, 2, 205)},
  {"84:F3:EB:10:30:C3", IPAddress(192, 168, 2, 206)},
  {"84:F3:EB:AC:DD:BA", IPAddress(192, 168, 2, 207)},
  {"50:02:91:EE:59:DC", IPAddress(192, 168, 2, 208)},
  {"08:F9:E0:5C:F3:81", IPAddress(192, 168, 2, 209)},
  {"CC:50:E3:F3:E1:D5", IPAddress(192, 168, 2, 210)},
  {"BC:DD:C2:5A:4E:6C", IPAddress(192, 168, 2, 211)},
  {"D8:BF:C0:0F:97:38", IPAddress(192, 168, 2, 212)},
  {"84:F3:EB:10:30:54", IPAddress(192, 168, 2, 213)},
  {"b4:e6:2d:55:39:93", IPAddress(192, 168, 2, 214)},
  {"8c:aa:b5:c4:f9:68", IPAddress(192, 168, 2, 215)}
  };
const int numEntries = sizeof(macIpTable) / sizeof(macIpTable[0]);

// IP defaults (unchanged)
IPAddress myIP(192, 168, 2, 250); // fallback
String myIPstring = "192.168.2.250";
IPAddress gatewayIP(192, 168, 2, 1);
IPAddress subnetIP(255, 255, 255, 0);
IPAddress MAXhostIP(192,168, 2,200);
const char* apSSID = "TheShed24";
const char* apPassword = "robmaudamelinesimonluc";

// UDP and OSC same as before
WiFiUDP UDP;
unsigned int listenPort = 8888;
unsigned int sendPort = 8887;
unsigned int debugPort = 8890;
char incomingPacket[255];
char msg[128];
int last_UDPheartbeat = 0;

OSCMessage debugMsg("/debug");
OSCMessage statusMsg("/status");
OSCMessage dataMsg("/data");
OSCMessage binaryMsg("/binary");

// sendUDPMessage templates unchanged (copy from your file)
template<typename T>
void sendUDPMessage(OSCMessage* msg_ptr, IPAddress receiver_IP, T first_msg) {
  (*msg_ptr).add(first_msg);
  UDP.beginPacket(receiver_IP, sendPort);
  (*msg_ptr).send(UDP);
  delay(100);
  (*msg_ptr).empty();
  UDP.endPacket();
}
template<typename T,typename... Args>
void sendUDPMessage(OSCMessage* msg_ptr, IPAddress receiver_IP, T first_msg, Args... rest) {
  (*msg_ptr).add(first_msg);
  sendUDPMessage(msg_ptr, receiver_IP, rest...);
}
void sendDebugUDP(const char* m) {
  UDP.beginPacket(MAXhostIP, debugPort);
  UDP.write((const uint8_t*)m, strlen(m));
  UDP.endPacket();
}

// buildDelayPattern same as before
void buildDelayPattern(float idealDelay) {
  uint32_t lowDelay = (uint32_t)floor(idealDelay);
  uint32_t highDelay = lowDelay + 1;
  float fractionHigh = ( idealDelay - (float)lowDelay );
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

// helper: update v_fixed when stripPeriod changes
void updateVFixedFromPeriod() {
  // v_fixed = total LED steps per period / seconds per period
  // total LED steps = STRIP_TICKS (equals 2*(NUM_LEDS-1)) -> number of index steps for full cycle
  float period_s = stripPeriod * 1e-6f; // stripPeriod is in microseconds
  if (period_s <= 0.0f) period_s = 1.0f;
  v_fixed = ((float)STRIP_TICKS) / period_s; // LEDs per second (covers full up+down cycle)
  // But our simBallPosition covers 0..NUM_LEDS-1; velocity sign / mapping handled by integrator.
}

// start/end show updated to initialize physics state & v_fixed
void startShow() {
  runShow = true;
  sendHeartbeat = false;
  sampleIndex = 0;
  numPeriods = 0;
  missedFrames = 0;
  totalMissedFrames = 0;
  buildDelayPattern(idealDelay);
  delayPatternIndex = 0;
  sampleIndex = 0;
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Starting Show | Position", ballPosition, "Direction", ballDirection, "Period", stripPeriod);
  epochStart64 = micros64();
  nextLEDupdate64 = epochStart64 + (uint64_t)delayPattern[0];

  // initialize physics state on start
  lastSimTime = epochStart64;
  timeAccumulator = 0ULL;

  // set continuous ball position to current integer (strip bottom by default)
  ballPosition = START_BALL_POSITION;
  simBallPosition = (float)(ballPosition - BALL_HALO);
  simBallVelocity = 0.0f;

  // compute v_fixed
  updateVFixedFromPeriod();
}

void endShow() {
  runShow = false;
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Stopping Show | Periods", numPeriods, "Missed Steps", totalMissedFrames);
}

// DIAG helpers (write_u32_be etc.) — copy from your file (omitted here for brevity in this paste)
// ... include the same write_u32_be/write_u16_be/write_u64_be/sendSamplesUDP/sendAllSamplesInChunks functions ...
// For brevity in this message I will assume they are the same as your original; include them in your file unchanged.

// ---------------------- NEW: physics integrator ----------------------
// dt_s is seconds per physics step (fixed)
void integratePhysics(float dt_s) {
  // Update velocity according to selected motion mode
  if (gravityMode) {
    simBallVelocity += (gravity * dt_s);
  } else { // CONSTANT_SWEEP
    // v_fixed is LEDs/sec for a full cycle; but simBallPosition uses 0..NUM_LEDS-1 range.
    // We want a sweep from top to bottom and back. v_fixed represents steps/sec across STRIP_TICKS.
    // Map v_fixed to LED-units/sec: steps across STRIP_TICKS correspond to 2*(NUM_LEDS-1) movement in index-space
    // but since simBallPosition moves in index space, v_fixed already matches index-step/sec if we treat it as such.
    // For sweep mode we simply set speed magnitude and preserve direction sign.
    // float mag = v_fixed; // LEDs/sec equivalent as computed
    // We want simBallVelocity to be ±mag/2? No — v_fixed computed as STRIP_TICKS / period_s so when simBallPosition goes up then down v_fixed matches steps.
    // Keep sign from ballDirection
    simBallVelocity = ballDirection * v_fixed;
  }

  // integrate position
  simBallPosition += (simBallVelocity * dt_s);

  // Bounce handling (reflect with restitution)
  bool bounced = false;
  bool bouncedAtBottom = false;
  if (simBallPosition > (float)(NUM_LEDS - 1 + BALL_HALO)) {
    // float over = simBallPosition - (float)(NUM_LEDS - 1 + BALL_HALO);
    // simBallPosition = (float)(NUM_LEDS - 1 + BALL_HALO) - over;
    simBallPosition = 2.0*(float)(NUM_LEDS - 1 + BALL_HALO) - simBallPosition;
    simBallVelocity = -simBallVelocity * RESTITUTION;
    bounced = true;
    bouncedAtBottom = true;
  }
  if (simBallPosition < 0.0f) {
    // float over = -simBallPosition;
    // simBallPosition = over;
    simBallPosition = -simBallPosition;
    simBallVelocity = -simBallVelocity * RESTITUTION;
    bounced = true;
    bouncedAtBottom = false;
  }

  // Update integer direction for compatibility with other code
  if (simBallVelocity > 0.0f) ballDirection = 1;
  else if (simBallVelocity < 0.0f) ballDirection = -1;

  // On bounce, set burst flag and set expiry (ms)
  if (bounced) {
    uint32_t nowMs = (uint32_t)(micros64() / 1000ULL);
    burstExpiresAtMs = nowMs + BURST_DURATION_MS;
    burstAtBottom = bouncedAtBottom;
    // increment numPeriods when hitting bottom (keep behaviour similar to previous)
    if (bouncedAtBottom) numPeriods++;
  }
}

// ---------------------- simplified moveBall for manual jogs ----------------------
void moveBallInt(int direction) {
  // change integer ballPosition (includes BALL_HALO) without bounce checks.
  // This function is used for manual jog only.
  int newPos = ballPosition + direction;
  if (newPos < BALL_HALO) newPos = BALL_HALO;
  if (newPos > BALL_HALO + NUM_LEDS - 1) newPos = BALL_HALO + NUM_LEDS - 1;
  ballPosition = newPos;
  // Also reflect this into continuous state to prevent jumps
  simBallPosition = (float)(ballPosition - BALL_HALO);
  // simBallVelocity = 0.0f;
}

void handleUDP() {
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;
    int intValue = atoi(&incomingPacket[1]); // convert the rest to integer
    float floatValue = atof(&incomingPacket[1]); // convert the rest to float

    switch (incomingPacket[0]) {
      case 'Z':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Latest Show Stats: Periods", numPeriods, "Missed Steps", totalMissedFrames);
      break;

      case 'G':
        if (intValue==0) flashBottomGradient = false;
        else { flashBottomGradient = true; bottomGradientLength = intValue; }
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Setting bottom gradient length", bottomGradientLength);
      break;

      case 'F':
        topGradientLength = intValue;
        flashTopGradient = !(topGradientLength==0);
        // if (intValue==0) flashTopGradient = false;
        // else { flashTopGradient = true; topGradientLength = intValue; }
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Setting top gradient length", topGradientLength);
      break;

      // set gravity (0 = sweep mode)
      case 'g':
        gravity = floatValue;
        gravityMode = (gravity>0);
      break;

      case 'B':
        sendHeartbeat = !sendHeartbeat;
      break;

      case 'H':
        ballHue = (uint8_t)intValue;
      break;

      case 'C':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Clearing LED strips");
        FastLED.clear(); FastLED.show(); delay(100);
      break;

      case 'P':
        stripPeriod = floatValue;
        if (stripPeriod<=0.0) {
          sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Got bad period:", stripPeriod);
          break;
        }
        idealDelay = (stripPeriod+0.5) / (float)STRIP_TICKS;
        sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Period to", stripPeriod, "Timesteps:", idealDelay);
        buildDelayPattern(idealDelay);
        updateVFixedFromPeriod(); // important to refresh constant-sweep velocity
      break;

      case 'S':
        if (intValue) startShow(); else endShow();
      break;

      case 'J':
        // Jog uses simplified integer move
        if (intValue < 0) {
          for (int t=0; t<abs(intValue); ++t) moveBallInt(-ballDirection);
        } else {
          for (int t=0; t<intValue; ++t) moveBallInt(ballDirection);
        }
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Jogging by", intValue);
      break;

      case 'V':
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Version Code:", versionCode);
      break;

      case 'X':
        ballPosition = (NUM_LEDS+BALL_HALO-1);
        ballDirection = -1;
        simBallPosition = (float)(ballPosition - BALL_HALO);
        simBallVelocity = 0.0f;
      break;

      case 'L':
        ballPosition = (NUM_LEDS+BALL_HALO-1) - intValue;
        ballDirection = (ballPosition == BALL_HALO+NUM_LEDS-1) ? -1 : 1;
        simBallPosition = (float)(ballPosition - BALL_HALO);
        simBallVelocity = 0.0f;
        FastLED.clear(); drawBallAtInteger(ballPosition); FastLED.show(); delay(100);
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Set Position:", ballPosition);
      break;

      // include other handlers and diagnostic commands identical to your original file...
      default:
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Received Unknown Message");
      break;
    }
  }
}

// ---------------------- setup() (kept mostly the same) ----------------------
void setup() {
  system_update_cpu_freq(160);
  pinMode(LED_PIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(DATA_PIN_SIDES, OUTPUT);
  digitalWrite(DATA_PIN, LOW);
  digitalWrite(DATA_PIN_SIDES, LOW);
  digitalWrite(LED_PIN, LED_ON);

  Serial.begin(115200);
  delay(1000);
  Serial.println("Serial up");

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(&leds[BALL_HALO], NUM_LEDS);
  FastLED.addLeds<WS2812, DATA_PIN_SIDES, GRB>(&ledsSides[BALL_HALO], NUM_LEDS);

  String mac = WiFi.macAddress(); mac.toUpperCase();
  Serial.print("Detected MAC: "); Serial.println(mac);
  bool found = false;
  for (int i = 0; i < numEntries; i++) {
    if (mac.equalsIgnoreCase(macIpTable[i].mac)) {
      myIP = macIpTable[i].ip;
      myIPstring = myIP.toString();
      found = true;
      break;
    }
  }
  Serial.print("Assigned IP: "); Serial.println(myIP);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.config(myIP, gatewayIP, subnetIP);

  Serial.print("Looking for AP \""); Serial.print(apSSID); Serial.println("\"...");
  bool foundAP = false;
  while (!foundAP) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == apSSID) { foundAP = true; break; }
    }
    if (!foundAP) Serial.print(".");
  }

  Serial.println("Found AP, attempting to connect...");
  WiFi.begin(apSSID, apPassword);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!"); Serial.print("Assigned IP: "); Serial.println(WiFi.localIP());

  if (WiFi.status() == WL_CONNECTED) {
    UDP.begin(listenPort);
    Serial.print("IP "); Serial.print(WiFi.localIP()); Serial.print(" listening on UDP port "); Serial.println(listenPort);
  }

  delay(500);
  Serial.println("Starting OTA...");
  String host = "D1mini_" + String(WiFi.localIP()[3]);
  ArduinoOTA.setHostname(host.c_str());
  ArduinoOTA.begin();
  Serial.println("OTA setup complete");

  buildDelayPattern(idealDelay);
  FastLED.clear(); FastLED.show(); delay(100);

  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Mac address", mac.c_str());
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Connected to", apSSID);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Listening for UDP on", listenPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Sending UDP on", sendPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Timestep:", stripDelay, "(us)");
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Finished setup.  Version code", versionCode);
  digitalWrite(LED_PIN, LED_OFF);
}

// ---------------------- main loop ----------------------
void loop() {
  handleUDP();

  if (sampleTiming && (sampleIndex > 0 && sampleIndex < DRIFT_SAMPLES)) {
    uint64_t now = micros64();
    uint64_t scheduledSample64 = samples64[0] + (uint64_t)(sampleIndex) * SAMPLE_INTERVAL_US;
    if (now >= scheduledSample64) {
      samples64[sampleIndex++] = now;
      if (sampleIndex >= DRIFT_SAMPLES) {
        sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Measurement BUFFER FULL", "Samples", sampleIndex);
        endShow();
      }
      yield();
    }
  }

  if (runShow) {
    
    uint64_t now = micros64();

    // if (lastSimTime == 0) lastSimTime = now;

    // compute delta in microseconds since last simulation, for comparison with Euler integration timestep SIM_DT
    uint64_t dt = now - lastSimTime;
    lastSimTime = now;

    // add up all the deltas to see if we need to integrate again
    // note: fixed timestep integrators are more stable in fast dynamics, which is why we don't just call
    // the dynamic equations with dt_us each time through the loop
    timeAccumulator += dt;

    // counter for simulation steps executed in this one simulation call
    // this should never run away but will guard against massive timeAccumulator values if they occur
    uint32_t totalSimSteps = 0;
    const uint32_t MAX_SIM_STEPS_SAFETY = 10000;
    while (timeAccumulator >= SIM_DT && totalSimSteps < MAX_SIM_STEPS_SAFETY) {
      integratePhysics((float)SIM_DT * 1e-6f);
      timeAccumulator -= SIM_DT;
      totalSimSteps++;
    }
    if (totalSimSteps >= MAX_SIM_STEPS_SAFETY) {
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Warning: physics catch-up cap hit", totalSimSteps);
    }

    // LED timing (Bresenham) — not used in dynamics simulation; determines WHEN to render frames
    uint64_t renderNow = micros64();
    missedFrames = 0;
    const uint32_t SAFETY_MAX_STEPS = 10000;
    while ((renderNow >= nextLEDupdate64) && (missedFrames < SAFETY_MAX_STEPS)) {
      uint32_t curDelay = delayPattern[delayPatternIndex];
      nextLEDupdate64 += (uint64_t)curDelay;
      delayPatternIndex++;
      if (delayPatternIndex >= STRIP_TICKS) {
        delayPatternIndex = 0;
      }
      missedFrames++;
    }
    if (missedFrames >= SAFETY_MAX_STEPS) {
      sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Warning: LED catch-up cap hit", (uint32_t)missedFrames);
    }

    // missedFrames = 1 is normal; if more than that, keep track of the total
    if (missedFrames > 0) totalMissedFrames += (missedFrames - 1);

    // Clear the strips before drawing
    FastLED.clear();

    // Determine integer index from continuous simBallPosition and reconcile ballPosition.
    int desiredInt = (int)roundf(simBallPosition);
    if (desiredInt < 0) desiredInt = 0;
    if (desiredInt > NUM_LEDS-1) desiredInt = NUM_LEDS-1;

    ballPosition = BALL_HALO + desiredInt;

    // Render burst/flash if active
    uint32_t nowMs = (uint32_t)(micros64() / 1000ULL);
    if (burstExpiresAtMs > nowMs) {
      // burst active — draw gradient based on bottom/top
      if (burstAtBottom && flashBottomGradient) {
        fill_gradient(&leds[BALL_HALO], NUM_LEDS-1-bottomGradientLength, CHSV(0,0,0), NUM_LEDS-1, CHSV(0,0,128));
      } else if (!burstAtBottom && flashTopGradient) {
        fill_gradient(&leds[BALL_HALO], 0, CHSV(0,0,128), topGradientLength, CHSV(0,0,0));
      }
    } else {
      // burst expired, nothing special (next draw will only draw ball)
    }

    // finally draw ball at its integer pos and show
    drawBallAtInteger(ballPosition);
    digitalWrite(DEBUG_PIN,HIGH);
    FastLED.show();
    digitalWrite(DEBUG_PIN,LOW);

  } else { // if show not running
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(apSSID, apPassword);
      while (WiFi.status() != WL_CONNECTED) delay(500);
    }
    ArduinoOTA.handle();
    FastLED.show();
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
