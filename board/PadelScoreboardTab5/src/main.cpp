/*
 * Padel Scoreboard for the M5Stack Tab5.
 * Same v1.1 match rules and the same ESP-NOW remotes as the 16x16 board.
 * The tablet draws the score. Tap a side to score, or use the remotes.
 *
 * Remote packet: cmd, team, data.
 *   0 score, 1 undo, 99 reset, 100 battery, 101 sleep, 102 deathmatch.
 * Team 1 is blue, team 2 is red.
 */

#include <M5Unified.h>

#include "display.h"
#include "game.h"
#include "radio.h"

#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint32_t kFlashMs = 160;
constexpr uint32_t kSummaryMs = 5000;
constexpr uint32_t kEndsMs = 2500;
constexpr uint32_t kHopMs = 500;
constexpr uint32_t kTapGapMs = 220;
constexpr uint32_t kBatteryMs = 60000;
constexpr int kBright = 255;
constexpr int kDim = 12;

Game gGame;
Phase gPhase = Phase::Live;
ScoreEffect gAfterFlash = ScoreEffect::Point;
bool gPendingEnds = false;
uint32_t gFlashUntil = 0;
uint32_t gPhaseUntil = 0;
bool gSummaryIsSet = false;
int gSummaryTeam = 0;
int gFlashTeam = 0;
int gColorMode = 0;
bool gBlackout = false;
int gHopX = 640;
int gHopY = 360;
bool gHopGreen = false;
uint32_t gLastHop = 0;
uint32_t gLastTap = 0;
uint32_t gLastBattery = 0;
uint32_t gDmStart = 0;
int gBoardBattery = -1;
int gRemoteBattery[2] = {-1, -1};
bool gRemoteSleep[2] = {false, false};
uint32_t gRemoteSeen[2] = {0, 0};
char gMac[24] = "";
int gChannel = 0;
bool gTransport = false;
bool gPipe = false;
uint16_t gAnimSeq = 0xFFFF;
bool gDirty = true;
int gAnim = 0;
uint32_t gLastAnim = 0;
char gSerialLine[32];
int gSerialLen = 0;

void restartClock() {
  gDmStart = millis();
  gGame.setClockSeconds(0);
}

void syncClockFromGame() { gDmStart = millis() - (uint32_t)gGame.clockSeconds() * 1000UL; }

void hop() {
  int w = M5.Display.width();
  int h = M5.Display.height();
  if (w < 2) w = 1280;
  if (h < 2) h = 720;
  int nx = random(w);
  int ny = random(h);
  if (nx == gHopX && ny == gHopY) nx = (nx + 1 + random(w - 1)) % w;
  gHopX = nx;
  gHopY = ny;
  gHopGreen = !gHopGreen;
  gLastHop = millis();
}

void applyScore(int team);

void notePipe() {
  if (gPipe) return;
  gPipe = true;
  Serial.println("C6 score pipe up");
}

void applyIncoming(const ScoreView &view) {
  gGame.applyView(view);
  if (view.channel) gChannel = view.channel;
  uint32_t now = millis();
  for (int i = 0; i < 2; i++) {
    gRemoteBattery[i] = view.remotePct[i];
    gRemoteSleep[i] = view.remoteSleep[i] != 0;
    if (view.remotePct[i] >= 0) gRemoteSeen[i] = now;
  }
  notePipe();
  if (view.flags & SCORE_F_FORCE_LIVE) {
    gPhase = Phase::Live;
    gPendingEnds = false;
  }
  ScoreEffect effect = (ScoreEffect)view.effect;
  if (effect == ScoreEffect::Ignored || view.seq == gAnimSeq) {
    gDirty = true;
    return;
  }
  gAnimSeq = view.seq;
  int team = view.effectTeam & 1;
  if (effect == ScoreEffect::ServeChosen) {
    gPhase = Phase::Live;
    Serial.printf("Serve started: team %d player 1\n", team + 1);
    gDirty = true;
    return;
  }
  gFlashTeam = team;
  gPendingEnds = view.changeEnds != 0;
  gAfterFlash = effect;
  gSummaryIsSet = effect == ScoreEffect::SetWon || effect == ScoreEffect::MatchWon;
  gSummaryTeam = team;
  gPhase = Phase::Flash;
  gFlashUntil = millis() + kFlashMs;
  gDirty = true;
}

void scoreSide(int team) {
  if (gPipe) {
    if (!radioSend(0, (uint8_t)(team + 1), 0)) Serial.println("C6 missed a score");
    return;
  }
  applyScore(team);
}

void sendOrLocal(uint8_t cmd, void (*local)()) {
  if (gPipe) {
    if (!radioSend(cmd, 1, 0)) Serial.println("C6 missed a command");
    return;
  }
  local();
}

void applyScore(int team) {
  ScoreResult result = gGame.score(team);
  if (result.effect == ScoreEffect::Ignored) return;
  if (result.effect == ScoreEffect::ServeChosen) {
    gPhase = Phase::Live;
    Serial.printf("Serve started: team %d player 1\n", team + 1);
    gDirty = true;
    return;
  }
  gFlashTeam = team;
  gPendingEnds = result.changeEnds;
  gAfterFlash = result.effect;
  gSummaryIsSet = result.effect == ScoreEffect::SetWon || result.effect == ScoreEffect::MatchWon;
  gSummaryTeam = team;
  gPhase = Phase::Flash;
  gFlashUntil = millis() + kFlashMs;
  gDirty = true;
}

void onUndo() {
  if (!gGame.undo()) return;
  syncClockFromGame();
  gPhase = Phase::Live;
  gDirty = true;
}

void onReset() {
  gGame.resetMatch();
  if (gGame.deathMatch()) restartClock();
  gPhase = Phase::Live;
  gDirty = true;
}

void onDeathmatch() {
  if (gGame.toggleDeathmatch()) restartClock();
  gPhase = Phase::Live;
  gDirty = true;
}

void handleLine(const char *line) {
  if (strncmp(line, "C6OTA ", 6) == 0) {
    radioOtaBegin((uint32_t)strtoul(line + 6, nullptr, 10));
    return;
  }
  if (strcmp(line, "SCORE 1") == 0) scoreSide(0);
  else if (strcmp(line, "SCORE 2") == 0) scoreSide(1);
  else if (strcmp(line, "UNDO") == 0) sendOrLocal(1, onUndo);
  else if (strcmp(line, "RESET") == 0) sendOrLocal(99, onReset);
  else if (strcmp(line, "DEATH") == 0) sendOrLocal(102, onDeathmatch);
}

void pollSerial() {
  if (radioOtaActive()) {
    radioOtaPump();
    return;
  }
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      gSerialLine[gSerialLen] = '\0';
      if (gSerialLen) handleLine(gSerialLine);
      gSerialLen = 0;
      continue;
    }
    if (gSerialLen < (int)sizeof(gSerialLine) - 1) gSerialLine[gSerialLen++] = c;
  }
}

void handleTouch() {
  int count = M5.Touch.getCount();
  for (int i = 0; i < count; i++) {
    auto touch = M5.Touch.getDetail(i);
    if (!touch.wasPressed()) continue;
    if (gBlackout) {
      gBlackout = false;
      M5.Display.setBrightness(kBright);
      gDirty = true;
      return;
    }
    uint32_t now = millis();
    if (now - gLastTap < kTapGapMs) return;
    gLastTap = now;
    Hit hit = displayHitTest(touch.x, touch.y);
    if ((hit == Hit::Score0 || hit == Hit::Score1) &&
        (gPhase == Phase::Summary || gPhase == Phase::Ends)) {
      gPhase = Phase::Live;
      gPendingEnds = false;
      gDirty = true;
      return;
    }
    switch (hit) {
      case Hit::Score0:
        if (gPhase != Phase::Won) scoreSide(0);
        break;
      case Hit::Score1:
        if (gPhase != Phase::Won) scoreSide(1);
        break;
      case Hit::Undo:
        sendOrLocal(1, onUndo);
        break;
      case Hit::Reset:
        sendOrLocal(99, onReset);
        break;
      case Hit::Death:
        sendOrLocal(102, onDeathmatch);
        break;
      case Hit::Color:
        gColorMode = (gColorMode + 1) % displayColorCount();
        gDirty = true;
        break;
      case Hit::Blackout:
        gBlackout = true;
        M5.Display.setBrightness(kDim);
        hop();
        gDirty = true;
        break;
      case Hit::None:
        break;
    }
  }
}

void advancePhase() {
  uint32_t now = millis();
  if (gPhase == Phase::Flash && now >= gFlashUntil) {
    if (gAfterFlash == ScoreEffect::MatchWon) gPhase = Phase::Won;
    else if (gAfterFlash == ScoreEffect::Game || gAfterFlash == ScoreEffect::SetWon) {
      gPhase = Phase::Summary;
      gPhaseUntil = now + kSummaryMs;
    } else {
      gPhase = Phase::Live;
    }
    gDirty = true;
  } else if (gPhase == Phase::Summary && now >= gPhaseUntil) {
    if (gPendingEnds) {
      gPhase = Phase::Ends;
      gPhaseUntil = now + kEndsMs;
    } else {
      gPhase = Phase::Live;
    }
    gDirty = true;
  } else if (gPhase == Phase::Ends && now >= gPhaseUntil) {
    gPhase = Phase::Live;
    gDirty = true;
  }
}

void tickClock() {
  if (gPipe || !gGame.deathMatch()) return;
  uint32_t elapsed = (millis() - gDmStart) / 1000UL;
  int seconds = elapsed > 3600 ? 3600 : (int)elapsed;
  if (seconds == gGame.clockSeconds()) return;
  gGame.setClockSeconds(seconds);
  if (!gBlackout) gDirty = true;
}

void tickBattery() {
  if (millis() - gLastBattery < kBatteryMs) return;
  gLastBattery = millis();
  int level = M5.Power.getBatteryLevel();
  if (level != gBoardBattery) {
    gBoardBattery = level;
    if (!gBlackout) gDirty = true;
  }
}

void render() {
  uint32_t now = millis();
  Chrome chrome;
  chrome.phase = gPhase;
  chrome.colorMode = gColorMode;
  chrome.blackout = gBlackout;
  chrome.hopX = gHopX;
  chrome.hopY = gHopY;
  chrome.hopGreen = gHopGreen;
  chrome.boardBattery = gBoardBattery;
  chrome.remoteBattery[0] = gRemoteBattery[0];
  chrome.remoteBattery[1] = gRemoteBattery[1];
  chrome.remoteSleep[0] = gRemoteSleep[0];
  chrome.remoteSleep[1] = gRemoteSleep[1];
  chrome.remoteStale[0] = gRemoteBattery[0] < 0 || (now - gRemoteSeen[0] > 300000UL);
  chrome.remoteStale[1] = gRemoteBattery[1] < 0 || (now - gRemoteSeen[1] > 300000UL);
  chrome.mac = gMac;
  chrome.channel = gChannel;
  chrome.radioOk = radioLinked();
  chrome.flashTeam = gFlashTeam;
  chrome.anim = gAnim;
  chrome.summaryIsSet = gSummaryIsSet;
  chrome.summaryTeam = gSummaryTeam;
  displayRender(gGame, chrome);
  gDirty = false;
}

}  // namespace

void setup() {
  // The USB CDC queue is 256 bytes unless this is set first. A larger OTA
  // write is dropped on the floor and the C6 never sees the image.
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.clear_display = true;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  if (M5.Display.width() < M5.Display.height()) M5.Display.setRotation(3);
  M5.Display.setBrightness(kBright);
  M5.setTouchButtonHeight(0);
  randomSeed(esp_random());

  displayBegin();
  gBoardBattery = M5.Power.getBatteryLevel();
  gLastBattery = millis();
  gTransport = radioBegin(gMac, sizeof(gMac), &gChannel);
  if (!gTransport) Serial.println("Score pipe transport did not start. Touch still scores.");
  gDirty = true;
  render();
}

void loop() {
  M5.update();
  ScoreView view;
  while (radioPoll(&view)) applyIncoming(view);
  pollSerial();
  handleTouch();
  advancePhase();
  tickClock();
  tickBattery();

  if (gBlackout && millis() - gLastHop >= kHopMs) {
    hop();
    gDirty = true;
  }
  if (!gBlackout && gPhase == Phase::Won && millis() - gLastAnim >= 80) {
    gAnim++;
    gLastAnim = millis();
    gDirty = true;
  }
  if (gDirty) render();
  delay(10);
}
