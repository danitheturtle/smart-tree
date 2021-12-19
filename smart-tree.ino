#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include "power_mgt.h"
#include "colorutils.h"

#ifndef STASSID
#define STASSID            ""
#define STAPSK             ""
#define LED_PIN            D2
#define INPUT_PIN          D4
#define NUM_LEDS           200
#endif

const char* host = "christmaslights";
const char* ssid = STASSID;
const char* password = STAPSK;
ESP8266WebServer server(80);

CRGB leds[NUM_LEDS];
CRGBPalette16 christmasPalette = {
  CRGB(0, 191, 49), CRGB(170, 0, 83), CRGB(200, 190, 130), CRGB(0, 99, 82),
  CRGB(96, 0, 75), CRGB(180, 50, 90), CRGB(0, 100, 150), CRGB(0, 0, 75),
  CRGB(0, 45, 86), CRGB(0, 158, 94), CRGB(183, 104, 0), CRGB(0, 58, 100),
  CRGB(220, 0, 25), CRGB(0, 191, 49), CRGB(0, 196, 182), CRGB(192, 108, 0)
};

//Light positions
int ledRotation[200] = {
  //lead wire
  -1, -1, -1, -1, -1, -1, 
  //row 0
  0, 0, 0, 0, 14, 28, 42, 55, 69, 83, 97, 111, 125, 138, 152, 166, 180, 194, 208, 222, 235, 249, 263, 277, 291, 305, 318, 332, 346,
  //row 1
  0, 16, 31, 47, 63, 78, 94, 110, 125, 141, 157, 172, 188, 203, 219, 235, 250, 266, 282, 297, 313, 329, 344,
  //row 2
  0, 16, 31, 47, 63, 78, 94, 110, 125, 141, 157, 172, 188, 203, 219, 235, 250, 266, 282, 297, 313, 329, 344,
  //row 3
  0, 20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320, 340,
  //row 4
  0, 23, 45, 68, 90, 113, 135, 158, 180, 203, 225, 248, 270, 293, 315, 338,
  //row 5
  0, 24, 48, 72, 96, 120, 144, 168, 192, 216, 240, 264, 288, 312, 336,
  //row 6
  0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330,
  //row 7
  0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330,
  //row 8
  0, 33, 65, 98, 131, 164, 196, 229, 262, 295, 327,
  //row 9
  0, 40, 80, 120, 160, 200, 240, 280, 320,
  //row 10
  0, 45, 90, 135, 180, 225, 270, 315,
  //row 11
  0, 45, 90, 135, 180, 225, 270, 315,
  //row 12
  0, 72, 144, 216, 288,
  //row 13
  0, 120, 240,
  //row 14
  90, 270
};
int rowIndex[15] = {6, 35, 58, 81, 99, 115, 130, 142, 154, 165, 174, 182, 190, 195, 198};

//modes
bool showRainbow = true;
bool showTwinkleWhite = false;
bool showPalette = false;
bool debugging = false;

//button
bool lastButtonPressed = false;
bool buttonPressed = false;

void rainbowUpdate() {
  static uint8_t startColorIndex = 0;
  for (uint8_t r=0; r<15; r++) {
    uint8_t rowLength = 0;
    if (r == 14) {
      rowLength = 2;
    } else {
      rowLength = rowIndex[r+1] - rowIndex[r];
    }
    uint8_t delta = 255 / rowLength;
    fill_rainbow(&leds[rowIndex[r]], (int)rowLength, startColorIndex++, delta);
  }
  FastLED.show();
  FastLED.delay(200);
}

void twinkleWhiteUpdate() {
  static uint8_t brightnessValues[NUM_LEDS];
  static bool hasTwinkleInit = false;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (!hasTwinkleInit) {
      brightnessValues[i] = (uint8_t)random(0, 255);
    } else {
      brightnessValues[i] = (uint8_t)min(255, max(0, (int)(brightnessValues[i] + random(-4.9, 5))));
    }
  }
  hasTwinkleInit = true;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(0, 0, brightnessValues[i]);
  }
  FastLED.show();
  FastLED.delay(50);
}

void paletteUpdate() {
  static uint8_t startIndex = 0;
  uint8_t colorIndex = startIndex % 16;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    //color palette accepts a range for colorIndex from 0-240 for some reason
    leds[i] = ColorFromPalette(christmasPalette, colorIndex*15, 255, NOBLEND);
    colorIndex = (colorIndex + 1) % 16;
  }
  FastLED.show();
  startIndex = (startIndex + 1) % NUM_LEDS;
  FastLED.delay(500);
}

void debuggingUpdate() {
  static uint8_t lightIndex = 0;
  if (lastButtonPressed && !buttonPressed) {
    lightIndex = lightIndex + 1;
  }
  FastLED.clear();
  leds[lightIndex] = CRGB::Red;
  //Draw a vertical line
  for (uint8_t i=0; i<15; i++) {
    leds[rowIndex[i]] = CRGB::White;
  }
  FastLED.show();
  FastLED.delay(50);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void updateButtonState() {
  lastButtonPressed = buttonPressed;
  int buttonInputVal = digitalRead(INPUT_PIN);
  if (buttonInputVal == 1) {
    buttonPressed = true;
  } else {
    buttonPressed = false;
  }
}

void setup() {  
  //ESP2866 Config
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(INPUT_PIN, INPUT);

  //Server setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    FastLED.delay(500);
    Serial.println(".");
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(host)) {
    Serial.println("MDNS responder started");
  }
  server.on("/rainbow", []() {
    showRainbow = true;
    showTwinkleWhite = false;
    showPalette = false;
    debugging = false;
    server.send(200, "text/plain", "started showing rainbow");
  });
  server.on("/twinkle", []() {
    showRainbow = false;
    showTwinkleWhite = true;
    showPalette = false;
    debugging = false;
    server.send(200, "text/plain", "started showing twinkles");
  });
  server.on("/palette", []() {
    showRainbow = false;
    showTwinkleWhite = false;
    showPalette = true;
    debugging = false;
    server.send(200, "text/plain", "started showing color palette");
  });
  server.on("/debugging", []() {
    showRainbow = false;
    showTwinkleWhite = false;
    showPalette = false;
    debugging = true;
    server.send(200, "text/plain", "started showing color palette");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  //LED setup
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(110);
}

// the loop function runs over and over again forever
void loop() {
  updateButtonState();
  server.handleClient();
  MDNS.update();
  if (showRainbow) {
    rainbowUpdate();
  } else if (showTwinkleWhite) {
    twinkleWhiteUpdate();
  } else if (showPalette) {
    paletteUpdate();
  } else if (debugging) {
    debuggingUpdate();
  }
}
