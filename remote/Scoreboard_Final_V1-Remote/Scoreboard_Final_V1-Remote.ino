// ===============================================
// Padel Scoreboard V1.0 - ESP32-C6 Remote
// FINAL VERSION (matches Scoreboard V1.0)
// Pins: LED 7, Button1 6 (team 1), Button2 20 (team 2)
// ===============================================

#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>

#define BUTTON1_PIN 6
#define BUTTON2_PIN 20
#define LED_PIN 7                 // WS2812 on remote
#define BATTERY_PIN 0
#define NUM_LEDS 1

CRGB led[NUM_LEDS];

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Message {
  uint8_t cmd;      // 0 = score, 99 = reset, 100 = battery, 101 = sleep
  uint8_t team;     // 1 or 2
  uint8_t data;     // battery % or unused
};

bool btn1Pressed = false;
bool btn2Pressed = false;
unsigned long btn1Start = 0;
unsigned long btn2Start = 0;
unsigned long lastActivity = 0;
unsigned long lastBatterySend = 0;

void goToSleep();

void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  // Optional: flash green when data is sent successfully
  if (status == ESP_NOW_SEND_SUCCESS) {
    led[0] = CRGB::Green;
    FastLED.show();
    delay(80);
    led[0] = CRGB::Black;
    FastLED.show();
  }
}

void sendMessage(uint8_t cmd, uint8_t team, uint8_t data = 0) {
  Message msg;
  msg.cmd = cmd;
  msg.team = team;
  msg.data = data;

  esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
  lastActivity = millis();
}

int getBatteryPercent() {
  int raw = analogRead(BATTERY_PIN);
  int percent = map(raw, 1600, 2130, 0, 100);
  return constrain(percent, 0, 100);
}

void goToSleep() {
  sendMessage(101, 1);
  sendMessage(101, 2);
  delay(800);                    // give time for message to transmit
  led[0] = CRGB::Yellow;
  FastLED.show();
  delay(2000);
  led[0] = CRGB::Black;
  FastLED.show();
  while (digitalRead(BUTTON1_PIN) == LOW || digitalRead(BUTTON2_PIN) == LOW) delay(20);
  esp_deep_sleep_start();
}

void handlePress(unsigned long duration, uint8_t team) {
  if (duration < 2000) {
    // Short press → score point for this button's team
    sendMessage(0, team);
  } else if (duration < 4000) {
    // 2–4 seconds → reset
    sendMessage(99, team);
  } else {
    // 4+ seconds → sleep
    goToSleep();
  }
}

void handleButtons() {
  bool b1 = digitalRead(BUTTON1_PIN) == LOW;
  if (b1 && !btn1Pressed) {
    btn1Pressed = true;
    btn1Start = millis();
  }
  if (!b1 && btn1Pressed) {
    btn1Pressed = false;
    handlePress(millis() - btn1Start, 1);
  }

  bool b2 = digitalRead(BUTTON2_PIN) == LOW;
  if (b2 && !btn2Pressed) {
    btn2Pressed = true;
    btn2Start = millis();
  }
  if (!b2 && btn2Pressed) {
    btn2Pressed = false;
    handlePress(millis() - btn2Start, 2);
  }
}

void setup() {
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(led, NUM_LEDS);
  FastLED.setBrightness(40);

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Send initial battery for both teams
  int bat = getBatteryPercent();
  sendMessage(100, 1, bat);
  sendMessage(100, 2, bat);
  lastBatterySend = millis();
  lastActivity = millis();

  led[0] = CRGB(0, 30, 255);
  FastLED.show();
  delay(400);
  led[0] = CRGB(255, 10, 20);
  FastLED.show();
  delay(400);
  led[0] = CRGB::Black;
  FastLED.show();
}

void loop() {
  handleButtons();

  // Send battery every 60 seconds
  if (millis() - lastBatterySend > 60000) {
    int bat = getBatteryPercent();
    sendMessage(100, 1, bat);
    sendMessage(100, 2, bat);
    lastBatterySend = millis();
  }

  // Inactivity sleep (20 minutes)
  if (millis() - lastActivity > 1200000) {
    goToSleep();
  }

  // Team color heartbeat every 5 seconds
  static unsigned long lastFlash = 0;
  if (millis() - lastFlash > 5000) {
    led[0] = CRGB(0, 30, 255);
    FastLED.show();
    delay(80);
    led[0] = CRGB(255, 10, 20);
    FastLED.show();
    delay(80);
    led[0] = CRGB::Black;
    FastLED.show();
    lastFlash = millis();
  }

  delay(10);
}