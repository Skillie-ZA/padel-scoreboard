// ===============================================
// Padel Scoreboard V1.0 - ESP32-C3 Main Board
// FINAL VERSION - Rainbow + PADEL PIXEL startup + all features
// ===============================================

#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>

#define LED_PIN 6
#define MODE_PIN 7
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define BRIGHTNESS 64
#define ZIGZAG true
#define MIRROR true
#define REVERSE_FONT_BITS true

CRGB leds[NUM_LEDS];
CRGB team1Color = CRGB(0, 30, 255);
CRGB team2Color = CRGB(255, 10, 20);
CRGB deathTeam1Color = CRGB(100, 0, 255);
CRGB deathTeam2Color = CRGB(255, 80, 0);
CRGB fadedTeam1 = CRGB(0, 4, 15);
CRGB fadedTeam2 = CRGB(15, 1, 3);
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
unsigned long dmStartTime = 0;
int dmSeconds = 420;

long lastBatteryTime[2] = {0,0};
int batteryPercent[2] = {-1,-1};
bool remoteSleep[2] = {false,false};

struct Message { uint8_t cmd, team, data; };

// ================== FUNCTIONS ==================

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Message msg; memcpy(&msg, data, sizeof(msg));
  int t = msg.team-1; if(t<0||t>1) return;
  switch(msg.cmd){
    case 0: scorePoint(t); break;
    case 99: 
      resetAll(); 
      if (deathMatchMode) { dmStartTime = millis(); dmSeconds = 420; }
      break;
    case 100: batteryPercent[t]=msg.data; lastBatteryTime[t]=millis(); remoteSleep[t]=false; break;
    case 101: remoteSleep[t]=true; break;
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
  if (MIRROR) y = MATRIX_HEIGHT - 1 - y;
  if (ZIGZAG && (y % 2 == 1)) x = MATRIX_WIDTH - 1 - x;
  return y * MATRIX_WIDTH + x;
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

void drawBatteryBars(int team) {
  int perc = batteryPercent[team];
  bool sleeping = remoteSleep[team];
  long timeSince = millis() - lastBatteryTime[team];
  CRGB barColor = (perc == -1 || timeSince > 300000) ? orangeColor : (sleeping ? yellowColor : greenColor);
  int bars = (perc >= 0) ? (perc / 20) + 1 : 0;
  bars = constrain(bars, 0, 5);
  int startX = (team == 0) ? 0 : MATRIX_WIDTH - 5;
  int bottomY = MATRIX_HEIGHT - 1;
  for (int i = 0; i < 5; i++) {
    leds[getLED(startX + i, bottomY)] = (i < bars) ? barColor : blackColor;
  }
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
  dmSeconds = 420;
}

const char* getPointStr(int team) {
  if (inTiebreak) { static char buf[3]; sprintf(buf, "%02d", points[team]); return buf; }
  if (points[team] < 3) { const char* strs[] = {"00","15","30"}; return strs[points[team]]; }
  if (advantageTeam == -1 && deuceCount >= 2) return "SP";
  if (advantageTeam == team) return "AD";
  return "40";
}

void drawFadedBackground() {
  for (int x = 0; x < 8; x++) for (int y = 0; y < 16; y++) leds[getLED(x,y)] = fadedTeam1;
  for (int x = 8; x < 16; x++) for (int y = 0; y < 16; y++) leds[getLED(x,y)] = fadedTeam2;
}

void drawHorizontalFadedBackground(bool topIsTeam1) {
  CRGB top = topIsTeam1 ? fadedTeam1 : fadedTeam2;
  CRGB bottom = topIsTeam1 ? fadedTeam2 : fadedTeam1;
  for (int y = 0; y < 8; y++) for (int x = 0; x < 16; x++) leds[getLED(x,y)] = top;
  for (int y = 8; y < 16; y++) for (int x = 0; x < 16; x++) leds[getLED(x,y)] = bottom;
}

void drawMainScreen() {
  if (matchOver) { drawMatchOverScreen(); return; }
  if (deathMatchMode) {
    drawHorizontalFadedBackground(true);
    int m = dmSeconds / 60;
    int s = dmSeconds % 60;
    drawSmallDigit(2, 2, m, deathTeam1Color);
    leds[getLED(6, 3)] = whiteColor; leds[getLED(6, 4)] = whiteColor;
    drawSmallDigit(8, 2, s/10, deathTeam2Color);
    drawSmallDigit(12, 2, s%10, deathTeam2Color);
    char buf1[3]; sprintf(buf1, "%02d", points[0]);
    char buf2[3]; sprintf(buf2, "%02d", points[1]);
    drawSmallText(0, 10, buf1, deathTeam1Color);
    drawSmallText(9, 10, buf2, deathTeam2Color);
  } else {
    drawFadedBackground();
    drawLargeDigit(1, 0, games[0][currentSet], team1Color);
    drawLargeDigit(10, 0, games[1][currentSet], team2Color);
    drawSmallText(0, 10, getPointStr(0), team1Color);
    drawSmallText(9, 10, getPointStr(1), team2Color);
  }
  drawBatteryBars(0);
  drawBatteryBars(1);
}

void drawSecondaryScreen() {
  bool team1Leads = (sets[0] >= sets[1]);
  drawHorizontalFadedBackground(team1Leads);
  int offsetY = 3;
  int startX = 2;
  if (team1Leads) {
    drawSmallDigit(startX, offsetY, games[0][0], team1Color);
    drawSmallDigit(startX+4, offsetY, games[0][1], team1Color);
    drawSmallDigit(startX+8, offsetY, games[0][2], team1Color);
    drawSmallDigit(startX, offsetY+6, games[1][0], team2Color);
    drawSmallDigit(startX+4, offsetY+6, games[1][1], team2Color);
    drawSmallDigit(startX+8, offsetY+6, games[1][2], team2Color);
  } else {
    drawSmallDigit(startX, offsetY, games[1][0], team2Color);
    drawSmallDigit(startX+4, offsetY, games[1][1], team2Color);
    drawSmallDigit(startX+8, offsetY, games[1][2], team2Color);
    drawSmallDigit(startX, offsetY+6, games[0][0], team1Color);
    drawSmallDigit(startX+4, offsetY+6, games[0][1], team1Color);
    drawSmallDigit(startX+8, offsetY+6, games[0][2], team1Color);
  }
  for (int x = 0; x < 16; x++) { leds[getLED(x,0)] = greenColor; leds[getLED(x,15)] = greenColor; }
  for (int y = 0; y < 16; y++) { leds[getLED(0,y)] = greenColor; leds[getLED(15,y)] = greenColor; }
}

void drawMatchOverScreen() {
  bool winnerIsTeam1 = (winner == 0);
  drawHorizontalFadedBackground(winnerIsTeam1);
  int offsetY = 3;
  int startX = 2;
  if (winnerIsTeam1) {
    drawSmallDigit(startX, offsetY, games[0][0], team1Color);
    drawSmallDigit(startX+4, offsetY, games[0][1], team1Color);
    drawSmallDigit(startX+8, offsetY, games[0][2], team1Color);
    drawSmallDigit(startX, offsetY+6, games[1][0], team2Color);
    drawSmallDigit(startX+4, offsetY+6, games[1][1], team2Color);
    drawSmallDigit(startX+8, offsetY+6, games[1][2], team2Color);
  } else {
    drawSmallDigit(startX, offsetY, games[1][0], team2Color);
    drawSmallDigit(startX+4, offsetY, games[1][1], team2Color);
    drawSmallDigit(startX+8, offsetY, games[1][2], team2Color);
    drawSmallDigit(startX, offsetY+6, games[0][0], team1Color);
    drawSmallDigit(startX+4, offsetY+6, games[0][1], team1Color);
    drawSmallDigit(startX+8, offsetY+6, games[0][2], team1Color);
  }
  CRGB winCol = winnerIsTeam1 ? team1Color : team2Color;
  for (int cycle = 0; cycle < 5; cycle++) {
    for (int offset = 0; offset < 8; offset++) {
      for (int x = 0; x < 16; x++) { leds[getLED(x,0)] = blackColor; leds[getLED(x,15)] = blackColor; }
      for (int y = 0; y < 16; y++) { leds[getLED(0,y)] = blackColor; leds[getLED(15,y)] = blackColor; }
      for (int x = 0; x < 16; x++) {
        CRGB c = ((x + offset) % 2 == 0) ? winCol : whiteColor;
        leds[getLED(x,0)] = c;
        leds[getLED(x,15)] = c;
      }
      for (int y = 1; y < 15; y++) {
        CRGB c = ((y + offset) % 2 == 0) ? winCol : whiteColor;
        leds[getLED(0,y)] = c;
        leds[getLED(15,y)] = c;
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
  if (deathMatchMode) {
    points[team] = constrain(points[team] + 1, 0, 99);
    drawMainScreen(); FastLED.show();
    return;
  }
  int other = 1 - team;
  if (inTiebreak) {
    points[team]++;
    if (points[team] >= 7 && points[team] - points[other] >= 2) handleGameWon(team);
    return;
  }
  if (points[team] < 3) { points[team]++; return; }
  if (points[other] < 3) { handleGameWon(team); return; }
  if (advantageTeam == -1) {
    if (deuceCount >= 2) handleGameWon(team);
    else advantageTeam = team;
  } else {
    if (advantageTeam == team) handleGameWon(team);
    else { advantageTeam = -1; deuceCount++; }
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

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  pinMode(MODE_PIN, INPUT_PULLUP);
  
  rainbowAnimation();
  padelPixelPanelAndAnimation();
  
  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);
  resetAll();
  drawMainScreen();
  FastLED.show();
}

void loop() {
  static bool lastState = HIGH;
  bool state = digitalRead(MODE_PIN);
  if (state == LOW && lastState == HIGH) {
    deathMatchMode = !deathMatchMode;
    if (deathMatchMode) {
      dmStartTime = millis();
      dmSeconds = 420;
      resetAll();
    }
    drawMainScreen(); FastLED.show();
    delay(300);
  }
  lastState = state;

  if (deathMatchMode) {
    unsigned long elapsed = (millis() - dmStartTime) / 1000;
    int remaining = 420 - elapsed;
    if (remaining < 0) remaining = 0;
    if (remaining != dmSeconds) {
      dmSeconds = remaining;
      drawMainScreen();
      FastLED.show();
    }
  }
  delay(100);
}