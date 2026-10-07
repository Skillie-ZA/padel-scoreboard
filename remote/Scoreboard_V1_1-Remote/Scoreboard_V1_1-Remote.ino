// ===============================================
// Padel Scoreboard V1.1 - ESP32-C6 Remote
// One remote per team. Set TEAM to 1 (blue) or 2 (red).
//
// Pins: Button 1 GPIO 6 | Button 2 GPIO 20 | WS2812 GPIO 7 | Battery GPIO 0
// ESP32-C6 deep-sleep wake only works on GPIO 0-7, so Button 1 (GPIO 6) wakes.
//
// Button 1: <2s score this team | 2-4s reset | 4s+ sleep
// Button 2: 0.5-2s undo (or score other team if Other Team Mode)
//           5-7s toggle deathmatch | 10s+ toggle Other Team Mode
// ===============================================

#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>

#define TEAM 2                    // 1 = blue remote, 2 = red remote
// Wide 8x32 scoreboard.
// Blue remote flashed 2026-10-07: FC:01:2C:EB:D7:20
// Red remote flashed 2026-10-07: FC:01:2C:EE:30:70
// Earlier pair for FC:01:2C:EB:E9:AC was left as-is:
//   blue FC:01:2C:EE:34:04, red FC:01:2C:EC:11:E8

#define BUTTON1_PIN 6
#define BUTTON2_PIN 20
#define LED_PIN 7
#define BATTERY_PIN 0
#define NUM_LEDS 1

#define BATTERY_SEND_INTERVAL 60000UL
#define INACTIVITY_TIMEOUT 1200000UL
#define BREATH_PERIOD_MS 5000UL
#define BREATH_HALF_MS 600UL

CRGB led[NUM_LEDS];
CRGB teamColor = (TEAM == 1) ? CRGB(0, 30, 255) : CRGB(255, 10, 20);

// Wide 8x32 only. Wi-Fi STA MAC read from that ESP32-C3.
// ESP-NOW uses this 6-byte STA MAC, not an EUI-64 form.
uint8_t scoreboardAddress[] = {0x1C, 0xDB, 0xD4, 0x34, 0x85, 0xBC};

struct Message {
  uint8_t cmd;
  uint8_t team;
  uint8_t data;
};

bool btn1Pressed = false;
bool btn2Pressed = false;
unsigned long btn1Start = 0;
unsigned long btn2Start = 0;
unsigned long lastActivity = 0;
unsigned long lastBatterySend = 0;
unsigned long lastBreath = 0;
bool otherTeamMode = false;
bool sendingFlash = false;

void goToSleep();

void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    sendingFlash = true;
    led[0] = CRGB::Green;
    FastLED.show();
    delay(80);
    led[0] = CRGB::Black;
    FastLED.show();
    sendingFlash = false;
  }
}

void sendMessage(uint8_t cmd, uint8_t team, uint8_t data = 0) {
  Message msg = {cmd, team, data};
  esp_now_send(scoreboardAddress, (uint8_t *)&msg, sizeof(msg));
}

int getBatteryPercent() {
  int raw = analogRead(BATTERY_PIN);
  int percent = map(raw, 1600, 2130, 0, 100);
  return constrain(percent, 0, 100);
}

uint64_t wakeupMask() {
  uint64_t mask = 0;
  if (BUTTON1_PIN <= 7) mask |= 1ULL << BUTTON1_PIN;
  if (BUTTON2_PIN <= 7) mask |= 1ULL << BUTTON2_PIN;
  if (mask == 0) mask = 1ULL << 6;
  return mask;
}

void enableButtonWakeup() {
  esp_deep_sleep_enable_gpio_wakeup(wakeupMask(), ESP_GPIO_WAKEUP_GPIO_LOW);
}

void goToSleep() {
  sendMessage(101, TEAM);
  delay(800);
  led[0] = CRGB::Yellow;
  FastLED.show();
  delay(1500);
  led[0] = CRGB::Black;
  FastLED.show();
  delay(50);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  while (digitalRead(BUTTON1_PIN) == LOW || digitalRead(BUTTON2_PIN) == LOW) delay(20);

  enableButtonWakeup();
  Serial.println("Sleeping. Press Button 1 (GPIO 6) to wake.");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

void handleButtons() {
  bool b1 = digitalRead(BUTTON1_PIN) == LOW;
  if (b1 && !btn1Pressed) {
    btn1Pressed = true;
    btn1Start = millis();
  }
  if (!b1 && btn1Pressed) {
    unsigned long dur = millis() - btn1Start;
    btn1Pressed = false;
    lastActivity = millis();

    if (dur < 2000) {
      sendMessage(0, TEAM);
    } else if (dur < 4000) {
      sendMessage(99, TEAM);
    } else {
      goToSleep();
    }
  }

  bool b2 = digitalRead(BUTTON2_PIN) == LOW;
  if (b2 && !btn2Pressed) {
    btn2Pressed = true;
    btn2Start = millis();
  }
  if (!b2 && btn2Pressed) {
    unsigned long dur = millis() - btn2Start;
    btn2Pressed = false;
    lastActivity = millis();

    if (dur >= 10000) {
      otherTeamMode = !otherTeamMode;
      Serial.printf("Other Team Mode %s\n", otherTeamMode ? "ON" : "OFF");
      led[0] = otherTeamMode ? CRGB::Purple : teamColor;
      FastLED.show();
      delay(250);
      led[0] = CRGB::Black;
      FastLED.show();
    } else if (dur >= 5000 && dur < 7000) {
      sendMessage(102, TEAM);
      Serial.println("Toggle deathmatch");
    } else if (dur >= 500 && dur < 2000) {
      if (otherTeamMode) {
        uint8_t other = (TEAM == 1) ? 2 : 1;
        sendMessage(0, other);
        Serial.println("Score other team");
      } else {
        sendMessage(1, TEAM);
        Serial.println("Undo last point");
      }
    }
  }
}

void updateBreathing() {
  if (sendingFlash) return;
  unsigned long now = millis();
  unsigned long t = now - lastBreath;
  if (t >= BREATH_PERIOD_MS + 2 * BREATH_HALF_MS) {
    lastBreath = now;
    led[0] = CRGB::Black;
    FastLED.show();
    return;
  }
  if (t < BREATH_PERIOD_MS) return;

  unsigned long phase = t - BREATH_PERIOD_MS;
  uint8_t scale;
  if (phase <= BREATH_HALF_MS) {
    scale = (uint8_t)(phase * 255UL / BREATH_HALF_MS);
  } else if (phase <= 2 * BREATH_HALF_MS) {
    scale = (uint8_t)((2 * BREATH_HALF_MS - phase) * 255UL / BREATH_HALF_MS);
  } else {
    return;
  }
  CRGB c = teamColor;
  c.nscale8(scale);
  led[0] = c;
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== Remote v1.1 TEAM %d ===\n", TEAM);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  enableButtonWakeup();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(led, NUM_LEDS);
  FastLED.setBrightness(40);
  led[0] = CRGB::Black;
  FastLED.show();

  WiFi.mode(WIFI_STA);
  Serial.print("Remote MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.printf("Master: %02X:%02X:%02X:%02X:%02X:%02X\n",
                scoreboardAddress[0], scoreboardAddress[1], scoreboardAddress[2],
                scoreboardAddress[3], scoreboardAddress[4], scoreboardAddress[5]);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  }
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, scoreboardAddress, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("add_peer failed");
  }

  sendMessage(100, TEAM, getBatteryPercent());
  lastBatterySend = lastActivity = lastBreath = millis();

  led[0] = teamColor;
  FastLED.show();
  delay(400);
  led[0] = CRGB::Black;
  FastLED.show();
  Serial.println("Ready");
}

void loop() {
  handleButtons();
  updateBreathing();

  if (millis() - lastBatterySend > BATTERY_SEND_INTERVAL) {
    sendMessage(100, TEAM, getBatteryPercent());
    lastBatterySend = millis();
  }

  if (millis() - lastActivity > INACTIVITY_TIMEOUT) {
    goToSleep();
  }

  delay(10);
}
