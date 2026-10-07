// ===============================================
// Padel Scoreboard Wide 8x32 - ESP32-C3
// Same v1.1 remote protocol. This chip's own MAC is printed on Serial.
// Remotes already hardcoded for this board keep working. Do not retarget them.
//
// GPIO 20 = WS2812 data | GPIO 5 = mode button and deep-sleep wake
// Button: <1s cycle colours | 1-3s deathmatch | 3s+ deep sleep
// Deep sleep also after 40 minutes with no play. Battery pings do not keep it awake.
// GPIO 5 is an RTC pin, so a press to GND wakes the chip. The match resets on wake.
//
// Matrix: 8 high x 32 wide, 256 LEDs. Column serpentine, top-left is LED 0.
// First 8 LEDs are the full left column, top to bottom. Odd columns run
// bottom to top, so the strip continues along the bottom (bottom left, then
// bottom right) and finishes toward the top right.
//
// Screen, left to right, blue at the top left:
//   cols 0-7 blue game | 8-15 blue points | 16-23 red points | 24-31 red game
// Color modes: 0 both white | 1 blue/red teams | 2+ shared rainbow
// ESP-NOW: 0 score, 1 undo, 99 reset, 100 battery, 101 sleep, 102 deathmatch
// ===============================================

#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

#define LED_PIN 20
#define BUTTON_PIN 5
#define NUM_LEDS 256
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define BRIGHTNESS 64
#define REVERSE_FONT_BITS true

#define REMOTE_BATTERY_LEFT_X 0
#define REMOTE_BATTERY_RIGHT_X 27
#define BATTERY_READ_INTERVAL 60000
#define DEATHMATCH_HOLD_MS 1000
#define SLEEP_HOLD_MS 3000
#define INACTIVITY_TIMEOUT 2400000UL  // 40 minutes
#define DM_MAX_SECONDS 3600            // 60 minutes

#define COLOR_MODE_WHITE 0
#define COLOR_MODE_TEAMS 1
const CRGB rainbowColors[] = {
  CRGB::Orange, CRGB::Yellow, CRGB::Green,
  CRGB::Cyan, CRGB::Blue, CRGB::Purple, CRGB::Red
};
#define NUM_COLOR_MODES (2 + (sizeof(rainbowColors) / sizeof(rainbowColors[0])))

CRGB leds[NUM_LEDS];
CRGB team1Color = CRGB(0, 30, 255);
CRGB team2Color = CRGB(255, 10, 20);
CRGB deathTeam1Color = CRGB(100, 0, 255);
CRGB deathTeam2Color = CRGB(255, 80, 0);
CRGB fadedTeam1 = CRGB(0, 4, 15);
CRGB fadedTeam2 = CRGB(15, 1, 3);
CRGB dimTeam1 = CRGB(0, 12, 45);
CRGB dimTeam2 = CRGB(30, 5, 8);
CRGB dimWhite = CRGB(30, 30, 30);
CRGB warmWhite = CRGB(255, 240, 200);
CRGB greenColor = CRGB::Green;
CRGB yellowColor = CRGB::Yellow;
CRGB orangeColor = CRGB::Orange;
CRGB whiteColor = CRGB::White;
CRGB blackColor = CRGB::Black;

// PADEL PIXEL animation assets
CRGB colors[7] = {CRGB::Cyan, CRGB::Yellow, CRGB::Purple, CRGB::Green, CRGB::Red, CRGB::Blue, CRGB::Orange};

const uint8_t letterP_5x3[5][3] = {{1,1,1},{1,0,1},{1,1,1},{1,0,0},{1,0,0}};
const uint8_t letterA_5x3[5][3] = {{0,1,0},{1,0,1},{1,1,1},{1,0,1},{1,0,1}};
const uint8_t letterD_5x3[5][3] = {{1,1,0},{1,0,1},{1,0,1},{1,0,1},{1,1,0}};
const uint8_t letterE_5x3[5][3] = {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1}};
const uint8_t letterL_5x3[5][3] = {{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,1}};
const uint8_t letterI_5x3[5][3] = {{1,1,1},{0,1,0},{0,1,0},{0,1,0},{1,1,1}};
const uint8_t letterX_5x3[5][3] = {{1,0,1},{1,0,1},{0,1,0},{1,0,1},{1,0,1}};

const uint8_t (*lettersTopPadel[5])[3] = {letterP_5x3, letterA_5x3, letterD_5x3, letterE_5x3, letterL_5x3};
const uint8_t (*lettersBottomPixel[5])[3] = {letterP_5x3, letterI_5x3, letterX_5x3, letterE_5x3, letterL_5x3};

// Scoreboard fonts
const uint8_t smallFont[10][5] = {
  {0b111,0b101,0b101,0b101,0b111},{0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111},{0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001},{0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111},{0b111,0b001,0b001,0b001,0b001},
  {0b111,0b101,0b111,0b101,0b111},{0b111,0b101,0b111,0b001,0b111}
};

const uint8_t smallA[5] = {0b111,0b101,0b111,0b101,0b101};
const uint8_t smallD[5] = {0b110,0b101,0b101,0b101,0b110};
const uint8_t smallS[5] = {0b111,0b100,0b111,0b001,0b111};
const uint8_t smallP[5] = {0b111,0b101,0b111,0b100,0b100};
const uint8_t smallDash[5] = {0b000,0b000,0b011,0b000,0b000};

const uint8_t largeDigits[10][9] = {
  {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b10001,0b10001,0b01110},
  {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110},
  {0b01110,0b10001,0b00001,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111},
  {0b01110,0b10001,0b00001,0b00001,0b00110,0b00001,0b00001,0b10001,0b01110},
  {0b00010,0b00110,0b01010,0b10010,0b10010,0b11111,0b00010,0b00010,0b00010},
  {0b11111,0b10000,0b10000,0b11110,0b00001,0b00001,0b00001,0b10001,0b01110},
  {0b01110,0b10001,0b10000,0b10000,0b11110,0b10001,0b10001,0b10001,0b01110},
  {0b11111,0b00001,0b00001,0b00001,0b00010,0b00100,0b01000,0b10000,0b10000},
  {0b01110,0b10001,0b10001,0b10001,0b01110,0b10001,0b10001,0b10001,0b01110},
  {0b01110,0b10001,0b10001,0b10001,0b01111,0b00001,0b00001,0b10001,0b01110}
};

// 5x7, fits the 8-pixel height. Same bit order as largeDigits.
const uint8_t gameDigits[10][7] = {
  {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110},
  {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110},
  {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111},
  {0b01110,0b10001,0b00001,0b00110,0b00001,0b10001,0b01110},
  {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010},
  {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110},
  {0b01110,0b10000,0b10000,0b11110,0b10001,0b10001,0b01110},
  {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000},
  {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110},
  {0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b01110}
};

// Variables
int points[2] = {0,0};
int deuceCount = 0;
int advantageTeam = -1;
bool inTiebreak = false;
int games[2][3] = {{0}};
int currentSet = 0;
int sets[2] = {0,0};
bool matchOver = false;
int winner = -1;

bool deathMatchMode = false;
uint8_t colorMode = 0;
unsigned long dmStartTime = 0;
int dmSeconds = 0;

long lastBatteryTime[2] = {0,0};
int batteryPercent[2] = {-1,-1};
bool remoteSleep[2] = {false,false};
unsigned long lastBoardBatteryRead = 0;
unsigned long lastActivity = 0;

struct Message { uint8_t cmd, team, data; };

#define HISTORY_LEN 16
struct GameState {
  int points[2];
  int deuceCount;
  int advantageTeam;
  bool inTiebreak;
  int games[2][3];
  int currentSet;
  int sets[2];
  bool matchOver;
  int winner;
  bool deathMatchMode;
  int dmSeconds;
};
GameState history[HISTORY_LEN];
int historyCount = 0;

void pushHistory();
void undoPoint(int team);
void flashTeamColor(int team);
void scorePoint(int team);
void drawMainScreen();
void drawMatchOverScreen();
void handleGameWon(int team);
void resetAll();

// ================== FUNCTIONS ==================

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Message msg; memcpy(&msg, data, sizeof(msg));
  int t = msg.team-1; if(t<0||t>1) return;
  Serial.printf("ESP-NOW cmd=%u team=%u data=%u\n", msg.cmd, msg.team, msg.data);
  if (msg.cmd != 100 && msg.cmd != 101) lastActivity = millis();
  switch(msg.cmd){
    case 0:
      scorePoint(t);
      return;
    case 1:
      undoPoint(t);
      break;
    case 99:
      resetAll();
      if (deathMatchMode) { dmStartTime = millis(); dmSeconds = 0; }
      break;
    case 100: batteryPercent[t]=msg.data; lastBatteryTime[t]=millis(); remoteSleep[t]=false; break;
    case 101: remoteSleep[t]=true; break;
    case 102:
      deathMatchMode = !deathMatchMode;
      if (deathMatchMode) {
        dmStartTime = millis();
        dmSeconds = 0;
        resetAll();
      }
      break;
  }
  drawMainScreen();
  FastLED.show();
}

uint8_t reverseBits(uint8_t b, int bits) {
  uint8_t r = 0;
  for (int i = 0; i < bits; i++) { r = (r << 1) | (b & 1); b >>= 1; }
  return r;
}

int getLED(int x, int y) {
  if ((unsigned)x >= MATRIX_WIDTH || (unsigned)y >= MATRIX_HEIGHT) return 0;
  int yy = (x & 1) ? (MATRIX_HEIGHT - 1 - y) : y;
  return x * MATRIX_HEIGHT + yy;
}

void drawSmallChar(int x, int y, char ch, CRGB color) {
  const uint8_t* font;
  if (ch >= '0' && ch <= '9') font = smallFont[ch - '0'];
  else if (ch == 'A') font = smallA;
  else if (ch == 'D') font = smallD;
  else if (ch == 'S') font = smallS;
  else if (ch == 'P') font = smallP;
  else if (ch == '-') font = smallDash;
  else return;
  for (int row = 0; row < 5; row++) {
    uint8_t bits = font[row];
    if (REVERSE_FONT_BITS) bits = reverseBits(bits, 3);
    for (int col = 0; col < 3; col++) {
      if (bits & (1 << col)) leds[getLED(x + col, y + row)] = color;
    }
  }
}

void drawSmallText(int x, int y, const char* text, CRGB color) {
  int len = strlen(text);
  for (int i = 0; i < len; i++) drawSmallChar(x + i*4, y, text[i], color);
}

void drawSmallDigit(int x, int y, int digit, CRGB color) {
  drawSmallChar(x, y, '0' + digit, color);
}

void drawLargeDigit(int x, int y, int digit, CRGB color) {
  for (int row = 0; row < 9; row++) {
    uint8_t bits = largeDigits[digit][row];
    if (REVERSE_FONT_BITS) bits = reverseBits(bits, 5);
    for (int col = 0; col < 5; col++) {
      if (bits & (1 << col)) leds[getLED(x + col, y + row)] = color;
    }
  }
}

void drawGameDigit(int x, int y, int digit, CRGB color) {
  if (digit < 0) digit = 0;
  if (digit > 9) digit = 9;
  for (int row = 0; row < 7; row++) {
    uint8_t bits = gameDigits[digit][row];
    if (REVERSE_FONT_BITS) bits = reverseBits(bits, 5);
    for (int col = 0; col < 5; col++) {
      if (bits & (1 << col)) leds[getLED(x + col, y + row)] = color;
    }
  }
}

CRGB getRemoteBatteryColor(int perc, bool sleeping, long timeSince) {
  if (perc == -1 || timeSince > 300000) return orangeColor;
  if (sleeping) return yellowColor;
  return greenColor;
}

void drawBatteryBarAt(int startX, int perc, CRGB barColor) {
  int bars = (perc >= 0) ? (perc / 20) + 1 : 0;
  bars = constrain(bars, 0, 5);
  int bottomY = MATRIX_HEIGHT - 1;
  for (int i = 0; i < 5; i++) {
    leds[getLED(startX + i, bottomY)] = (i < bars) ? barColor : blackColor;
  }
}

void drawAllBatteryBars() {
  int perc0 = batteryPercent[0];
  long timeSince0 = millis() - lastBatteryTime[0];
  drawBatteryBarAt(REMOTE_BATTERY_LEFT_X, perc0,
                   getRemoteBatteryColor(perc0, remoteSleep[0], timeSince0));

  int perc1 = batteryPercent[1];
  long timeSince1 = millis() - lastBatteryTime[1];
  drawBatteryBarAt(REMOTE_BATTERY_RIGHT_X, perc1,
                   getRemoteBatteryColor(perc1, remoteSleep[1], timeSince1));
}

void resetAll() {
  memset(points, 0, sizeof(points));
  deuceCount = 0;
  advantageTeam = -1;
  inTiebreak = false;
  memset(games, 0, sizeof(games));
  currentSet = 0;
  memset(sets, 0, sizeof(sets));
  matchOver = false;
  winner = -1;
  dmSeconds = 0;
  historyCount = 0;
}

void captureState(GameState *s) {
  memcpy(s->points, points, sizeof(points));
  s->deuceCount = deuceCount;
  s->advantageTeam = advantageTeam;
  s->inTiebreak = inTiebreak;
  memcpy(s->games, games, sizeof(games));
  s->currentSet = currentSet;
  memcpy(s->sets, sets, sizeof(sets));
  s->matchOver = matchOver;
  s->winner = winner;
  s->deathMatchMode = deathMatchMode;
  s->dmSeconds = dmSeconds;
}

void restoreState(const GameState *s) {
  memcpy(points, s->points, sizeof(points));
  deuceCount = s->deuceCount;
  advantageTeam = s->advantageTeam;
  inTiebreak = s->inTiebreak;
  memcpy(games, s->games, sizeof(games));
  currentSet = s->currentSet;
  memcpy(sets, s->sets, sizeof(sets));
  matchOver = s->matchOver;
  winner = s->winner;
  deathMatchMode = s->deathMatchMode;
  dmSeconds = s->dmSeconds;
}

void pushHistory() {
  if (historyCount == HISTORY_LEN) {
    memmove(&history[0], &history[1], sizeof(GameState) * (HISTORY_LEN - 1));
    historyCount = HISTORY_LEN - 1;
  }
  captureState(&history[historyCount++]);
}

void undoPoint(int team) {
  (void)team;
  if (historyCount <= 0) return;
  restoreState(&history[--historyCount]);
  if (deathMatchMode) dmStartTime = millis() - (unsigned long)dmSeconds * 1000UL;
}

void flashTeamColor(int team) {
  CRGB c = (team == 0) ? team1Color : team2Color;
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
  delay(20);
}

const char* getPointStr(int team) {
  if (inTiebreak) { static char buf[3]; sprintf(buf, "%02d", points[team]); return buf; }
  if (points[team] < 3) { const char* strs[] = {"00","15","30"}; return strs[points[team]]; }
  if (advantageTeam == -1 && deuceCount >= 2) return "SP";
  if (advantageTeam == team) return "AD";
  return "40";
}

CRGB brighten40(CRGB color) {
  return CRGB(
    min(255, color.r * 14 / 10),
    min(255, color.g * 14 / 10),
    min(255, color.b * 14 / 10)
  );
}

CRGB getFadedTeam1() {
  return (colorMode == COLOR_MODE_TEAMS) ? fadedTeam1 : brighten40(fadedTeam1);
}

CRGB getFadedTeam2() {
  return (colorMode == COLOR_MODE_TEAMS) ? fadedTeam2 : brighten40(fadedTeam2);
}

void drawFadedBackground() {
  CRGB f1 = getFadedTeam1();
  CRGB f2 = getFadedTeam2();
  for (int x = 0; x < 16; x++) for (int y = 0; y < MATRIX_HEIGHT; y++) leds[getLED(x,y)] = f1;
  for (int x = 16; x < MATRIX_WIDTH; x++) for (int y = 0; y < MATRIX_HEIGHT; y++) leds[getLED(x,y)] = f2;
}

CRGB dimScoreColor(CRGB color) {
  return CRGB(color.r / 6, color.g / 6, color.b / 6);
}

CRGB getScoreColor(int team) {
  if (colorMode == COLOR_MODE_WHITE) return whiteColor;
  if (colorMode == COLOR_MODE_TEAMS) return (team == 0) ? team1Color : team2Color;
  return rainbowColors[colorMode - 2];
}

CRGB getSetScoreColor(int team, int set) {
  bool played = (set <= currentSet);
  CRGB color = getScoreColor(team);
  if (played) return color;
  if (colorMode == COLOR_MODE_WHITE) return dimWhite;
  if (colorMode == COLOR_MODE_TEAMS) return (team == 0) ? dimTeam1 : dimTeam2;
  return dimScoreColor(color);
}

void drawWideBorder(CRGB color) {
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    leds[getLED(x, 0)] = color;
    leds[getLED(x, MATRIX_HEIGHT - 1)] = color;
  }
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    leds[getLED(0, y)] = color;
    leds[getLED(MATRIX_WIDTH - 1, y)] = color;
  }
}

void drawMainScreen() {
  if (matchOver) { drawMatchOverScreen(); return; }
  drawFadedBackground();
  if (deathMatchMode) {
    // Outer pairs are the scores. Inner pairs are the clock.
    // The tens minute stays blank until 10:00, but its column is reserved.
    // The colon owns the two centre columns, so scores and seconds do not move.
    const int y = 1;
    int p0 = points[0]; if (p0 < 0) p0 = 0; if (p0 > 99) p0 = 99;
    int p1 = points[1]; if (p1 < 0) p1 = 0; if (p1 > 99) p1 = 99;
    char buf1[3]; sprintf(buf1, "%02d", p0);
    char buf2[3]; sprintf(buf2, "%02d", p1);
    drawSmallText(0, y, buf1, deathTeam1Color);

    int m = dmSeconds / 60;
    int s = dmSeconds % 60;
    if (m > 99) m = 99;
    if (m >= 10) drawSmallDigit(8, y, m / 10, greenColor);
    drawSmallDigit(12, y, m % 10, greenColor);
    leds[getLED(15, y + 1)] = greenColor;
    leds[getLED(16, y + 1)] = greenColor;
    leds[getLED(15, y + 3)] = greenColor;
    leds[getLED(16, y + 3)] = greenColor;
    drawSmallDigit(17, y, s / 10, greenColor);
    drawSmallDigit(21, y, s % 10, greenColor);

    drawSmallText(25, y, buf2, deathTeam2Color);
  } else {
    drawGameDigit(1, 0, games[0][currentSet], getScoreColor(0));
    drawSmallText(8, 1, getPointStr(0), getScoreColor(0));
    drawSmallText(17, 1, getPointStr(1), getScoreColor(1));
    drawGameDigit(26, 0, games[1][currentSet], getScoreColor(1));
  }
  drawAllBatteryBars();
}

void drawSetScores() {
  for (int set = 0; set < 3; set++) {
    drawSmallDigit(2 + set * 4, 1, games[0][set], getSetScoreColor(0, set));
    drawSmallDigit(19 + set * 4, 1, games[1][set], getSetScoreColor(1, set));
  }
}

void drawSecondaryScreen() {
  drawFadedBackground();
  drawSetScores();
  drawWideBorder(greenColor);
}

void drawMatchOverScreen() {
  bool winnerIsTeam1 = (winner == 0);
  drawFadedBackground();
  drawSetScores();
  CRGB winCol = winnerIsTeam1 ? team1Color : team2Color;
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int offset = 0; offset < 8; offset++) {
      drawWideBorder(blackColor);
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        CRGB c = ((x + offset) % 2 == 0) ? winCol : whiteColor;
        leds[getLED(x, 0)] = c;
        leds[getLED(x, MATRIX_HEIGHT - 1)] = c;
      }
      for (int y = 1; y < MATRIX_HEIGHT - 1; y++) {
        CRGB c = ((y + offset) % 2 == 0) ? winCol : whiteColor;
        leds[getLED(0, y)] = c;
        leds[getLED(MATRIX_WIDTH - 1, y)] = c;
      }
      FastLED.show();
      delay(75);
    }
  }
}

void drawSwapAnimation() {
  for (int i = 0; i < 3; i++) {
    for (int hue = 0; hue < 256; hue += 4) {
      fill_rainbow(leds, NUM_LEDS, hue, 8);
      FastLED.show();
      delay(20);
    }
  }
  fill_solid(leds, NUM_LEDS, blackColor);
  FastLED.show();
}

void handleGameWon(int team) {
  if (deathMatchMode) return;
  points[0] = points[1] = 0;
  deuceCount = 0;
  advantageTeam = -1;
  games[team][currentSet]++;
  int total = games[0][currentSet] + games[1][currentSet];
  bool needSwap = (total % 2 == 1);
  if (inTiebreak) {
    if (games[team][currentSet] >= 7 && games[team][currentSet] - games[1-team][currentSet] >= 2) {
      inTiebreak = false;
      sets[team]++;
      if (sets[team] >= 2) { matchOver = true; winner = team; }
      else currentSet++;
    }
  } else {
    if (games[team][currentSet] >= 6 && games[team][currentSet] - games[1-team][currentSet] >= 2) {
      sets[team]++;
      if (sets[team] >= 2) { matchOver = true; winner = team; }
      else currentSet++;
    } else if (games[0][currentSet] == 6 && games[1][currentSet] == 6) {
      inTiebreak = true;
    }
  }
  if (matchOver) {
    drawMatchOverScreen();
    FastLED.show();
  } else {
    drawSecondaryScreen();
    FastLED.show();
    delay(5000);
    if (needSwap) drawSwapAnimation();
    drawMainScreen();
    FastLED.show();
  }
}

void scorePoint(int team) {
  if (matchOver) return;
  pushHistory();
  bool wonGame = false;
  if (deathMatchMode) {
    points[team] = constrain(points[team] + 1, 0, 99);
  } else {
    int other = 1 - team;
    if (inTiebreak) {
      points[team]++;
      if (points[team] >= 7 && points[team] - points[other] >= 2) wonGame = true;
    } else if (points[team] < 3) {
      points[team]++;
    } else if (points[other] < 3) {
      wonGame = true;
    } else if (advantageTeam == -1) {
      if (deuceCount >= 2) wonGame = true;
      else advantageTeam = team;
    } else if (advantageTeam == team) {
      wonGame = true;
    } else {
      advantageTeam = -1;
      deuceCount++;
    }
  }
  flashTeamColor(team);
  if (wonGame) handleGameWon(team);
  else {
    drawMainScreen();
    FastLED.show();
  }
}

void rainbowAnimation() {
  for (int hue = 0; hue < 256; hue += 4) {
    fill_rainbow(leds, NUM_LEDS, hue, 8);
    FastLED.show();
    delay(20);
  }
  fill_solid(leds, NUM_LEDS, blackColor);
  FastLED.show();
}

void drawAbacus(int leftBeads[4], int rightBeads[4]) {
  for (int yy = 12; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) leds[getLED(xx, yy)] = CRGB::Black;
  for (int r = 0; r < 4; r++) {
    int yy = 12 + r;
    for (int b = 0; b < leftBeads[r]; b++)  leds[getLED(b, yy)] = CRGB::Red;
    for (int b = 0; b < rightBeads[r]; b++) leds[getLED(15 - b, yy)] = CRGB::Blue;
  }
}

void padelPixelPanelAndAnimation() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  for (int y = 0; y < 11; y++) {
    if (y < 5) {
      int row = y;
      for (int let = 0; let < 5; let++) {
        CRGB letterColor = colors[let % 7];
        for (int col = 0; col < 3; col++) {
          if (lettersTopPadel[let][row][col]) {
            int x = let * 3 + col;
            leds[getLED(x, y)] = letterColor;
          }
        }
      }
    }
    if (y >= 6 && y < 11) {
      int row = y - 6;
      for (int let = 0; let < 5; let++) {
        CRGB letterColor = colors[let % 7];
        for (int col = 0; col < 3; col++) {
          if (lettersBottomPixel[let][row][col]) {
            int x = let * 3 + col;
            leds[getLED(x, y)] = letterColor;
          }
        }
      }
    }
    FastLED.show();
    delay(500);
  }

  int leftBeads[4] = {5,5,0,0};
  int rightBeads[4] = {0,0,5,5};
  drawAbacus(leftBeads, rightBeads);
  FastLED.show();
  delay(1000);

  int totalMoves = 10;
  while (totalMoves > 0) {
    int simMoves = min((int)random(1,6), totalMoves);
    totalMoves -= simMoves;
    int selectedRods[5];
    for (int i = 0; i < simMoves; i++) {
      int r; do { r = random(4); } while (leftBeads[r] + rightBeads[r] == 0);
      selectedRods[i] = r;
    }
    int startX[5], endX[5], stepDir[5];
    bool fromLeft[5];
    for (int i = 0; i < simMoves; i++) {
      int r = selectedRods[i];
      fromLeft[i] = (leftBeads[r] > 0 && random(2));
      if (!fromLeft[i] && rightBeads[r] == 0) fromLeft[i] = true;
      if (fromLeft[i]) {
        startX[i] = leftBeads[r] - 1; leftBeads[r]--;
        endX[i] = 15 - rightBeads[r]; stepDir[i] = 1;
      } else {
        startX[i] = 15 - (rightBeads[r] - 1); rightBeads[r]--;
        endX[i] = leftBeads[r]; stepDir[i] = -1;
      }
    }
    int maxDist = 0;
    for (int i = 0; i < simMoves; i++) maxDist = max(maxDist, abs(endX[i] - startX[i]));
    for (int f = 1; f <= maxDist; f++) {
      drawAbacus(leftBeads, rightBeads);
      for (int i = 0; i < simMoves; i++) {
        if (f > abs(endX[i] - startX[i])) continue;
        int curX = startX[i] + f * stepDir[i];
        int yy = 12 + selectedRods[i];
        CRGB moveColor = (curX < 8) ? CRGB::Red : CRGB::Blue;
        leds[getLED(curX, yy)] = moveColor;
      }
      FastLED.show();
      delay(100);
    }
    for (int i = 0; i < simMoves; i++) {
      int r = selectedRods[i];
      if (fromLeft[i]) rightBeads[r]++; else leftBeads[r]++;
    }
    drawAbacus(leftBeads, rightBeads);
    FastLED.show();
    delay(300);
  }
  delay(5000);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(1000);
}

void startRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW init failed");
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_now_register_recv_cb(onReceive);
}

void enterSleep() {
  fill_solid(leds, NUM_LEDS, blackColor);
  FastLED.show();
  delay(30);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  while (digitalRead(BUTTON_PIN) == LOW) delay(20);

  Serial.println("Deep sleep. Press the button (GPIO 5) to wake.");
  Serial.flush();
  esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  if (err != ESP_OK) {
    Serial.printf("GPIO wakeup refused: %d\n", (int)err);
    return;
  }
  esp_deep_sleep_start();
}

void handleButton() {
  static bool buttonPressed = false;
  static unsigned long pressStart = 0;
  bool pressed = (digitalRead(BUTTON_PIN) == LOW);

  if (pressed && !buttonPressed) {
    buttonPressed = true;
    pressStart = millis();
  }

  if (!pressed && buttonPressed) {
    unsigned long duration = millis() - pressStart;
    buttonPressed = false;
    lastActivity = millis();

    if (duration >= SLEEP_HOLD_MS) {
      enterSleep();
    } else if (duration >= DEATHMATCH_HOLD_MS) {
      deathMatchMode = !deathMatchMode;
      if (deathMatchMode) {
        dmStartTime = millis();
        dmSeconds = 0;
        resetAll();
      }
      drawMainScreen();
      FastLED.show();
    } else if (duration > 50) {
      colorMode = (colorMode + 1) % NUM_COLOR_MODES;
      drawMainScreen();
      FastLED.show();
    }
  }
}

// The detector is already armed when this runs, which is before setup.
// A panel still latched on from the last image sags the rail and the
// stock handler resets the chip before setup can send black.
extern "C" void esp_brownout_init(void) {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}

static void enableBrownout() {
  uint32_t reg = RTC_CNTL_BROWN_OUT_ENA | RTC_CNTL_BROWN_OUT_RST_ENA;
  reg |= (0x3FFu << RTC_CNTL_BROWN_OUT_RST_WAIT_S);
  reg |= (1u << RTC_CNTL_BROWN_OUT_INT_WAIT_S);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, reg);
}

void printNetworkAddress() {
  Serial.println();
  Serial.println("=== Padel Scoreboard Wide 8x32 ===");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("uint8_t scoreboardAddress[] = {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X};\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("WiFi channel: %d\n", WiFi.channel());
  Serial.println("Paste the array into the remote sketch to hardcode this master.");
}

const uint8_t (*glyphFor(char ch))[3] {
  switch (ch) {
    case 'P': return letterP_5x3;
    case 'A': return letterA_5x3;
    case 'D': return letterD_5x3;
    case 'E': return letterE_5x3;
    case 'L': return letterL_5x3;
    case 'I': return letterI_5x3;
    case 'X': return letterX_5x3;
    default: return nullptr;
  }
}

// "PADEL PIXEL" scrolls in from the right and off the left.
// Each lit pixel keeps one random rainbow colour for the whole pass.
void scrollPadelPixel() {
  struct Lit {
    int8_t x;
    int8_t y;
    CRGB color;
  };
  Lit px[96];
  int n = 0;
  const char text[] = "PADEL PIXEL";
  int cursor = 0;
  bool gap = false;
  for (int i = 0; text[i] != '\0'; i++) {
    if (text[i] == ' ') {
      cursor += 3;
      gap = false;
      continue;
    }
    const uint8_t (*glyph)[3] = glyphFor(text[i]);
    if (!glyph) continue;
    if (gap) cursor += 1;
    gap = true;
    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 3; col++) {
        if (!glyph[row][col] || n >= 96) continue;
        px[n].x = cursor + col;
        px[n].y = row;
        px[n].color = CHSV(random(256), 255, 255);
        n++;
      }
    }
    cursor += 3;
  }

  const int y0 = 1;
  for (int shift = MATRIX_WIDTH; shift >= -cursor; --shift) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < n; i++) {
      int x = shift + px[i].x;
      int y = y0 + px[i].y;
      if ((unsigned)x >= MATRIX_WIDTH || (unsigned)y >= MATRIX_HEIGHT) continue;
      leds[getLED(x, y)] = px[i].color;
    }
    FastLED.show();
    delay(60);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void setup() {
  // Clear a latched panel before anything else. Only the letter pixels
  // light during the scroll, so the rail is not asked to drive a full panel.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(esp_random());
  scrollPadelPixel();

  Serial.begin(115200);
  unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 3000) delay(10);
  delay(100);

  lastBoardBatteryRead = millis();
  lastActivity = millis();

  startRadio();
  printNetworkAddress();

  resetAll();
  drawMainScreen();
  FastLED.show();
  delay(50);
  enableBrownout();
  printNetworkAddress();
}

void loop() {
  static bool macPrinted = false;
  if (!macPrinted && Serial) {
    printNetworkAddress();
    macPrinted = true;
  }

  handleButton();

  if (millis() - lastActivity > INACTIVITY_TIMEOUT) {
    enterSleep();
  }

  if (millis() - lastBoardBatteryRead > BATTERY_READ_INTERVAL) {
    lastBoardBatteryRead = millis();
    if (!matchOver) {
      drawMainScreen();
      FastLED.show();
    }
  }

  if (deathMatchMode) {
    unsigned long elapsed = (millis() - dmStartTime) / 1000;
    int elapsedSec = (elapsed > DM_MAX_SECONDS) ? DM_MAX_SECONDS : (int)elapsed;
    if (elapsedSec != dmSeconds) {
      dmSeconds = elapsedSec;
      drawMainScreen();
      FastLED.show();
    }
  }
  delay(100);
}