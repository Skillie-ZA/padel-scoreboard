#include "padel.h"

#include "game.h"
#include "score_link.h"

#include <string.h>

#include "esp_hosted_peer_data.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "padel";

static Game gGame;
static QueueHandle_t gCmdQ;
static uint16_t gSeq;
static int8_t gRemotePct[2] = {-1, -1};
static uint8_t gRemoteSleep[2];
static int64_t gRemoteSeenUs[2];
static int64_t gClockBaseUs;
static bool gNow;
static bool gRadioNoted;

static bool commandOk(uint8_t cmd, uint8_t team) {
  if (team != 1 && team != 2) return false;
  return cmd == 0 || cmd == 1 || cmd == 99 || cmd == 100 || cmd == 101 || cmd == 102;
}

static void publish(ScoreEffect effect, int team, bool changeEnds, bool forceLive) {
  ScoreView view;
  gGame.exportView(&view);
  view.seq = ++gSeq;
  view.effect = (uint8_t)effect;
  view.effectTeam = (uint8_t)(team < 0 ? 0 : team & 1);
  view.changeEnds = changeEnds ? 1 : 0;
  if (forceLive) view.flags |= SCORE_F_FORCE_LIVE;
  view.channel = 1;
  for (int i = 0; i < 2; i++) {
    int64_t age = esp_timer_get_time() - gRemoteSeenUs[i];
    bool stale = gRemotePct[i] < 0 || age > 300LL * 1000000LL;
    view.remotePct[i] = stale ? -1 : gRemotePct[i];
    view.remoteSleep[i] = gRemoteSleep[i];
  }
  esp_err_t err = esp_hosted_send_custom_data(SCORE_MSG_VIEW,
                                              reinterpret_cast<const uint8_t *>(&view), sizeof(view));
  if (err != ESP_OK && effect != ScoreEffect::Ignored) {
    ESP_LOGW(TAG, "snapshot send failed: %d", (int)err);
  }
}

static void restartClock() {
  gGame.setClockSeconds(0);
  gClockBaseUs = esp_timer_get_time();
}

static void syncClock() {
  gClockBaseUs = esp_timer_get_time() - (int64_t)gGame.clockSeconds() * 1000000LL;
}

static void tickClock() {
  if (!gGame.deathMatch()) return;
  int64_t elapsed = (esp_timer_get_time() - gClockBaseUs) / 1000000LL;
  if (elapsed < 0) elapsed = 0;
  if (elapsed > 3600) elapsed = 3600;
  gGame.setClockSeconds((int)elapsed);
}

static void applyCmd(const ScoreCmd &msg) {
  if (!commandOk(msg.cmd, msg.team)) return;
  int team = (int)msg.team - 1;
  switch (msg.cmd) {
    case 0: {
      ScoreResult result = gGame.score(team);
      publish(result.effect, team, result.changeEnds, false);
      break;
    }
    case 1:
      gGame.undo();
      syncClock();
      publish(ScoreEffect::Ignored, team, false, true);
      break;
    case 99:
      gGame.resetMatch();
      if (gGame.deathMatch()) restartClock();
      publish(ScoreEffect::Ignored, team, false, true);
      break;
    case 100:
      gRemotePct[team] = (int8_t)msg.data;
      gRemoteSleep[team] = 0;
      gRemoteSeenUs[team] = esp_timer_get_time();
      publish(ScoreEffect::Ignored, team, false, false);
      break;
    case 101:
      gRemoteSleep[team] = 1;
      publish(ScoreEffect::Ignored, team, false, false);
      break;
    case 102:
      if (gGame.toggleDeathmatch()) restartClock();
      publish(ScoreEffect::Ignored, team, false, true);
      break;
    default:
      break;
  }
}

static void onHostCmd(uint32_t msgId, const uint8_t *data, size_t len) {
  (void)msgId;
  if (!gCmdQ || !data || len < sizeof(ScoreCmd)) return;
  ScoreCmd msg;
  memcpy(&msg, data, sizeof(msg));
  xQueueSend(gCmdQ, &msg, 0);
}

static void onNow(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  (void)info;
  if (!gCmdQ || !data || len != 3) return;
  ScoreCmd msg;
  msg.cmd = data[0];
  msg.team = data[1];
  msg.data = data[2];
  xQueueSend(gCmdQ, &msg, 0);
}

static bool alreadyOn(esp_err_t err) {
  return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
}

static void ensureRadio() {
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) != ESP_OK) return;

  if (!gNow) {
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (!alreadyOn(esp_wifi_set_mode(WIFI_MODE_STA))) return;
    if (!alreadyOn(esp_wifi_start())) return;
    esp_wifi_disconnect();
    esp_wifi_set_ps(WIFI_PS_NONE);
    if (esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE) != ESP_OK) return;
    if (esp_now_init() != ESP_OK) {
      if (!gRadioNoted) ESP_LOGW(TAG, "ESP-NOW init failed");
      gRadioNoted = true;
      return;
    }
    if (esp_now_register_recv_cb(onNow) != ESP_OK) {
      ESP_LOGW(TAG, "ESP-NOW callback failed");
      return;
    }
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memset(peer.peer_addr, 0xFF, 6);
    peer.channel = 1;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "ESP-NOW on channel 1, STA %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    gNow = true;
    // Confirm only after the radio is up. A boot loop before this can still roll back
    // when the factory bootloader has rollback enabled. OTA does not replace that bootloader.
    esp_err_t mark = esp_ota_mark_app_valid_cancel_rollback();
    if (mark != ESP_OK && mark != ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
      ESP_LOGW(TAG, "rollback confirm failed: %d", (int)mark);
    }
    publish(ScoreEffect::Ignored, -1, false, false);
    return;
  }

  static uint16_t relock = 0;
  if (++relock < 50) return;
  relock = 0;
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary != 1) {
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  }
}

static void padelTask(void *arg) {
  (void)arg;
  int64_t lastBeat = 0;
  for (;;) {
    ensureRadio();
    ScoreCmd msg;
    while (gCmdQ && xQueueReceive(gCmdQ, &msg, 0) == pdTRUE) applyCmd(msg);
    tickClock();
    int64_t now = esp_timer_get_time();
    if (gNow && now - lastBeat > 500000) {
      lastBeat = now;
      publish(ScoreEffect::Ignored, -1, false, false);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

extern "C" void padel_init(void) {
  gCmdQ = xQueueCreate(24, sizeof(ScoreCmd));
  if (!gCmdQ) {
    ESP_LOGE(TAG, "command queue failed");
    return;
  }
  esp_err_t err = esp_hosted_register_custom_callback(SCORE_MSG_CMD, onHostCmd);
  if (err != ESP_OK) ESP_LOGW(TAG, "host command callback failed: %d", (int)err);
  if (xTaskCreate(padelTask, "padel", 8192, nullptr, 5, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "task create failed");
  }
}
