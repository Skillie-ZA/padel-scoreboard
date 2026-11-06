#include "radio.h"

#include <WiFi.h>
#include <esp_err.h>
#include <esp_hosted.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp32-hal-hosted.h>

extern "C" {
esp_err_t esp_hosted_slave_ota_write(uint8_t *ota_data, uint32_t ota_data_len);
}

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <stdio.h>
#include <string.h>

static QueueHandle_t gQueue = nullptr;
static bool gTransport = false;
static volatile uint32_t gLastViewMs = 0;

static bool gOta = false;
static uint32_t gOtaLeft = 0;
static uint32_t gOtaGot = 0;
static uint32_t gOtaLastMs = 0;
static uint8_t gOtaBuf[1024];
static size_t gOtaFill = 0;

static void onView(uint32_t msgId, const uint8_t *data, size_t len) {
  (void)msgId;
  if (!gQueue || !data || len != sizeof(ScoreView)) return;
  ScoreView view;
  memcpy(&view, data, sizeof(view));
  if (view.version != SCORE_LINK_VERSION) return;
  gLastViewMs = millis();
  xQueueSend(gQueue, &view, 0);
}

static bool fail(const char *why) {
  Serial.println(why);
  gTransport = false;
  return false;
}

bool radioBegin(char *macOut, size_t macLen, int *channelOut) {
  if (macOut && macLen) macOut[0] = '\0';
  if (channelOut) *channelOut = 1;
  gTransport = false;
  gLastViewMs = 0;

  gQueue = xQueueCreate(8, sizeof(ScoreView));
  if (!gQueue) return fail("Score queue failed");

  // STA brings the SDIO transport up. The C6 owns the channel and does not join an AP.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, true);
  esp_wifi_set_ps(WIFI_PS_NONE);
  Serial.setTimeout(50);
  delay(100);

  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) WiFi.macAddress(mac);
  if (macOut && macLen) {
    snprintf(macOut, macLen, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
  }

  Serial.println();
  Serial.println("=== Padel Scoreboard Tab5 ===");
  Serial.println("Match runs on the ESP32-C6. This tablet only draws it.");
  uint32_t hostMaj = 0, hostMin = 0, hostPat = 0;
  uint32_t slaveMaj = 0, slaveMin = 0, slavePat = 0;
  hostedGetHostVersion(&hostMaj, &hostMin, &hostPat);
  hostedGetSlaveVersion(&slaveMaj, &slaveMin, &slavePat);
  Serial.printf("esp-hosted host %u.%u.%u, C6 slave %u.%u.%u\n", hostMaj, hostMin, hostPat, slaveMaj,
                slaveMin, slavePat);
  Serial.printf("C6 MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4],
                mac[5]);
  Serial.printf("uint8_t scoreboardAddress[] = {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X};\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("Paste the array into the v1.1 remote sketch.");

  esp_err_t err = esp_hosted_register_custom_callback(SCORE_MSG_VIEW, onView);
  if (err != ESP_OK) {
    Serial.printf("C6 score pipe failed: %s (%d)\n", esp_err_to_name(err), (int)err);
    return fail("C6 score pipe did not start");
  }

  gTransport = true;
  Serial.println("Waiting for the C6 score pipe");
  return true;
}

bool radioLinked() {
  if (!gTransport || gLastViewMs == 0) return false;
  return millis() - gLastViewMs < 3000;
}

bool radioPoll(ScoreView *out) {
  if (!gQueue || !out) return false;
  return xQueueReceive(gQueue, out, 0) == pdTRUE;
}

bool radioSend(uint8_t cmd, uint8_t team, uint8_t data) {
  if (!gTransport) return false;
  ScoreCmd msg;
  msg.cmd = cmd;
  msg.team = team;
  msg.data = data;
  return esp_hosted_send_custom_data(SCORE_MSG_CMD, reinterpret_cast<const uint8_t *>(&msg),
                                     sizeof(msg)) == ESP_OK;
}

bool radioOtaActive() { return gOta; }

void radioOtaBegin(uint32_t size) {
  if (gOta) return;
  if (!gTransport) {
    Serial.println("C6OTA FAIL link");
    return;
  }
  if (size < 1024 || size > 1800 * 1024) {
    Serial.println("C6OTA FAIL size");
    return;
  }
  // The SDIO driver floods USB while an update is in progress and hides the result.
  esp_log_level_set("H_SDIO_DRV", ESP_LOG_NONE);
  esp_log_level_set("sdmmc_io", ESP_LOG_NONE);
  if (!hostedBeginUpdate()) {
    Serial.println("C6OTA FAIL begin");
    Serial.flush();
    return;
  }
  gOta = true;
  gOtaLeft = size;
  gOtaGot = 0;
  gOtaFill = 0;
  gOtaLastMs = millis();
  Serial.println("C6OTA READY");
  Serial.flush();
}

void radioOtaPump() {
  if (!gOta) return;
  while (gOtaLeft && Serial.available()) {
    size_t room = sizeof(gOtaBuf) - gOtaFill;
    if (room > gOtaLeft) room = gOtaLeft;
    int avail = Serial.available();
    if (avail <= 0) break;
    if ((size_t)avail < room) room = (size_t)avail;
    size_t got = Serial.readBytes(reinterpret_cast<char *>(gOtaBuf + gOtaFill), room);
    if (got == 0) break;
    gOtaFill += got;
    gOtaLeft -= got;
    gOtaGot += got;
    gOtaLastMs = millis();
    if (gOtaFill == sizeof(gOtaBuf) || gOtaLeft == 0) {
      uint32_t chunk = (uint32_t)gOtaFill;
      Serial.printf("C6OTA WRITE %u\n", chunk);
      Serial.flush();
      uint32_t started = millis();
      esp_err_t err = esp_hosted_slave_ota_write(gOtaBuf, chunk);
      uint32_t took = millis() - started;
      gOtaLastMs = millis();
      if (err != ESP_OK) {
        Serial.printf("C6OTA FAIL write %s (%d) after %ums\n", esp_err_to_name(err), (int)err, took);
        Serial.flush();
        gOta = false;
        return;
      }
      Serial.printf("C6OTA WROTE %u %ums\n", gOtaGot, took);
      Serial.flush();
      gOtaFill = 0;
    }
  }
  if (gOtaLeft == 0) {
    Serial.println("C6OTA END");
    Serial.flush();
    if (!hostedEndUpdate()) {
      Serial.println("C6OTA FAIL end");
      Serial.flush();
      gOta = false;
      return;
    }
    Serial.println("C6OTA ACTIVATE");
    Serial.flush();
    if (!hostedActivateUpdate()) {
      Serial.println("C6OTA FAIL activate");
      Serial.flush();
      gOta = false;
      return;
    }
    Serial.println("C6OTA OK");
    Serial.flush();
    gOta = false;
    delay(200);
    esp_restart();
  }
  if (millis() - gOtaLastMs > 8000) {
    Serial.println("C6OTA FAIL timeout");
    gOta = false;
  }
}
