#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <stdarg.h>
#include <ArduinoOTA.h>

// LED blinking interrupt stuff
const int LED_PIN = LED_BUILTIN;   // Active-LOW on Wemos D1 mini/lite
Ticker blinkTicker;
Ticker cycleTicker;

volatile int blinkCode = 0;        // 0–10, sets number of flashes per cycle
volatile int currentFlash = 0;
volatile bool ledState = false;

// LED timing constants
const float FLASH_INTERVAL = 0.05; // seconds per half-cycle → 10 Hz full blink
const float CYCLE_PERIOD = 2.0;    // seconds total per code cycle

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
void startBlinkCycle() {
  currentFlash = 0;
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

// --- Connection state tracking ---
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

  // if (msg_ptr!=&dataMsg) {
  //   (*msg_ptr).send(Serial);     // this isn't very pretty but works
  //   Serial.println();
  // }

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

  // attach LED timer interrupts
  blinkTicker.attach(FLASH_INTERVAL, toggleLedISR);
  cycleTicker.attach(CYCLE_PERIOD, startBlinkCycle);
  setBlinkCode(2);      // 2 flashes

  // Serial debugging start
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // off

  delay(1000);
  Serial.println();

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

  // int attempt = 0;
  // while (WiFi.status() != WL_CONNECTED && attempt < 30) {
  //   // blinkLed(200); // keep fast blink while connecting
  //   delay(500);
  //   attempt++;
  // }

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

  // Set up listener for OTA programming
  // String host = "D1mini_" + String(WiFi.localIP()[3]);
  // ArduinoOTA.setHostname(host.c_str());
  // ArduinoOTA.begin();
  // Serial.println("OTA setup complete");

  // Note sendUDPMessage can't handle String types directly; append .c_str() to send a *char
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Mac address", mac.c_str());
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Connected to", apSSID);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Listening for UDP on", listenPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Sending UDP on", sendPort);
  sendUDPMessage(&debugMsg, MAXhostIP, myIPstring.c_str(), "Finished setup.");
  
}

void loop() {
  // activate OTA programming
  ArduinoOTA.handle();
  
  // --- Receive UDP packets and parse them ---
  int packetSize = UDP.parsePacket();

  if (packetSize) {
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) incomingPacket[len] = 0;

    Serial.print("I got one! ");
    Serial.println(incomingPacket);

    if (strcmp(incomingPacket,"zero")==0) {
      setBlinkCode(0);
    }

    if (incomingPacket[0] == 'F') {
      int value = atoi(&incomingPacket[1]); // convert the rest to integer
      
      sendUDPMessage(&statusMsg, MAXhostIP, myIPstring.c_str(), "Setting Flashes to", value);

      Serial.print("Setting LED flashes to: ");
      Serial.println(value);
      setBlinkCode(value);
    }
  }
}
