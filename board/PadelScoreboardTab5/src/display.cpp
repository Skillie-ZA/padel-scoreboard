#include "display.h"

#include <M5Unified.h>

#include <stdio.h>

namespace {

constexpr int kHeader = 84;
constexpr int kFooter = 90;

struct Layout {
  int w = 1280;
  int h = 720;
  int footerY = 720 - kFooter;
};

Layout gLayout;
M5Canvas *gCanvas = nullptr;
bool gSprite = false;
int gFont2H = 16;
int gFont4H = 26;
int gFont8H = 75;
int gFont8Digit = 55;

uint16_t kHeaderBg, kBluePanel, kRedPanel, kDeathL, kDeathR, kWhite, kBlack, kYellow, kGreen;
uint16_t kOrange, kNet, kDim, kBlue, kRed;

const char *kModeName[] = {"WHITE", "TEAMS", "ORANGE", "YELLOW", "GREEN",
                           "CYAN",  "BLUE",  "PURPLE", "RED"};

struct Rgb {
  uint8_t r, g, b;
};

Rgb kRainbow[] = {
    {255, 140, 0}, {255, 220, 0}, {0, 210, 60}, {0, 210, 210}, {50, 110, 255}, {190, 60, 255}, {255, 40, 50},
};

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return M5.Display.color565(r, g, b); }

void scoreRgb(int mode, int team, Rgb *out) {
  if (mode <= 0) {
    *out = {255, 255, 255};
    return;
  }
  if (mode == 1) {
    *out = team == 0 ? Rgb{0, 150, 255} : Rgb{255, 55, 65};
    return;
  }
  int i = mode - 2;
  if (i < 0) i = 0;
  if (i > 6) i = 6;
  *out = kRainbow[i];
}

void buttonRect(int index, int *x, int *y, int *w, int *h) {
  int slot = gLayout.w / 5;
  *x = index * slot + 8;
  *y = gLayout.footerY + 12;
  *w = slot - 16;
  *h = kFooter - 24;
}

bool font8Safe(const char *text) {
  if (!text || !text[0]) return false;
  for (const char *p = text; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c == ' ' || c == ':' || c == '-' || c == '.') continue;
    if (c >= '0' && c <= '9') continue;
    return false;
  }
  return true;
}

template <class Gfx>
void measure(Gfx &d) {
  d.setTextSize(1);
  d.setFont(&fonts::Font2);
  gFont2H = d.fontHeight();
  d.setFont(&fonts::Font4);
  gFont4H = d.fontHeight();
  d.setFont(&fonts::Font8);
  gFont8H = d.fontHeight();
  gFont8Digit = d.textWidth("0");
  if (gFont2H < 8) gFont2H = 16;
  if (gFont4H < 8) gFont4H = 26;
  if (gFont8H < 8) gFont8H = 75;
  if (gFont8Digit < 8) gFont8Digit = 55;
}

int fitScale(int fontH, int maxPx, int want) {
  if (fontH < 1) fontH = 1;
  int cap = maxPx / fontH;
  if (cap < 1) cap = 1;
  if (want > cap) want = cap;
  if (want < 1) want = 1;
  return want;
}

template <class Gfx>
void label(Gfx &d, const lgfx::IFont *font, int size, uint16_t fg, uint16_t bg, textdatum_t datum,
           int x, int y, const char *text) {
  d.setFont(font);
  d.setTextSize(size);
  d.setTextColor(fg, bg);
  d.setTextDatum(datum);
  d.drawString(text, x, y);
}

template <class Gfx>
void drawBlock(Gfx &d, const char *text, int size, uint16_t fg, uint16_t bg, int x, int y) {
  if (font8Safe(text)) label(d, &fonts::Font8, size, fg, bg, middle_center, x, y, text);
  else label(d, &fonts::Font4, size, fg, bg, middle_center, x, y, text);
}

uint16_t batteryColor(int pct, bool sleeping, bool stale, bool board) {
  if (!board) {
    if (pct < 0 || stale) return kOrange;
    if (sleeping) return kYellow;
    return kGreen;
  }
  if (pct < 0) return kDim;
  if (pct >= 60) return kGreen;
  if (pct >= 20) return kYellow;
  return kOrange;
}

template <class Gfx>
void drawBattery(Gfx &d, int x, int y, const char *name, int pct, uint16_t col) {
  d.fillRoundRect(x, y, 132, 56, 8, kHeaderBg);
  d.drawRoundRect(x, y, 132, 56, 8, col);
  char line[16];
  if (pct < 0) snprintf(line, sizeof(line), "%s  --", name);
  else snprintf(line, sizeof(line), "%s %d%%", name, pct);
  label(d, &fonts::Font2, 1, col, kHeaderBg, middle_left, x + 8, y + 18, line);
  int bars = 0;
  if (pct >= 0) {
    bars = pct / 20 + 1;
    if (bars > 5) bars = 5;
  }
  for (int i = 0; i < 5; i++) {
    d.fillRoundRect(x + 10 + i * 22, y + 34, 16, 12, 2, i < bars ? col : kDim);
  }
}

template <class Gfx>
void drawHeader(Gfx &d, const Game &game, const Chrome &chrome) {
  d.fillRect(0, 0, gLayout.w, kHeader, kHeaderBg);
  label(d, &fonts::Font4, 1, kWhite, kHeaderBg, top_left, 16, 8, "PADEL");
  char mac[48];
  if (!chrome.radioOk) {
    if (chrome.mac && chrome.mac[0]) snprintf(mac, sizeof(mac), "NO LINK  %s", chrome.mac);
    else snprintf(mac, sizeof(mac), "NO LINK");
  } else if (chrome.mac && chrome.mac[0])
    snprintf(mac, sizeof(mac), "%s  CH%d", chrome.mac, chrome.channel);
  else
    snprintf(mac, sizeof(mac), "CH%d", chrome.channel);
  label(d, &fonts::Font2, 1, chrome.radioOk ? kDim : kOrange, kHeaderBg, top_left, 16, 48, mac);

  int setsL = 240;
  int setsR = gLayout.w - 16 - 132 * 3 - 24 - 16;
  if (setsR < setsL + 180) setsR = setsL + 180;
  int setsMid = (setsL + setsR) / 2;
  int spacing = (setsR - setsL) / 4;
  if (spacing > 150) spacing = 150;
  if (spacing < 80) spacing = 80;
  if (game.deathMatch()) {
    label(d, &fonts::Font4, 1, kGreen, kHeaderBg, middle_center, setsMid, 36, "DEATHMATCH");
  } else {
    for (int set = 0; set < 3; set++) {
      int x = setsMid + (set - 1) * spacing;
      Rgb a, b;
      scoreRgb(chrome.colorMode, 0, &a);
      scoreRgb(chrome.colorMode, 1, &b);
      bool played = set <= game.currentSet();
      uint16_t ca = played ? rgb(a.r, a.g, a.b) : rgb(a.r / 3, a.g / 3, a.b / 3);
      uint16_t cb = played ? rgb(b.r, b.g, b.b) : rgb(b.r / 3, b.g / 3, b.b / 3);
      char left[4], right[4];
      snprintf(left, sizeof(left), "%d", game.games(0, set));
      snprintf(right, sizeof(right), "%d", game.games(1, set));
      label(d, &fonts::Font4, 1, ca, kHeaderBg, middle_right, x - 18, 32, left);
      label(d, &fonts::Font4, 1, kWhite, kHeaderBg, middle_center, x, 32, "-");
      label(d, &fonts::Font4, 1, cb, kHeaderBg, middle_left, x + 18, 32, right);
      if (set == game.currentSet() && !game.matchOver()) {
        d.fillRect(x - 28, 58, 56, 4, kYellow);
      }
    }
  }

  int bx = gLayout.w - 16 - 132;
  drawBattery(d, bx, 14, "R", chrome.remoteBattery[1],
              batteryColor(chrome.remoteBattery[1], chrome.remoteSleep[1], chrome.remoteStale[1], false));
  bx -= 144;
  drawBattery(d, bx, 14, "TAB", chrome.boardBattery,
              batteryColor(chrome.boardBattery, false, false, true));
  bx -= 144;
  drawBattery(d, bx, 14, "L", chrome.remoteBattery[0],
              batteryColor(chrome.remoteBattery[0], chrome.remoteSleep[0], chrome.remoteStale[0], false));
}

template <class Gfx>
void drawFooter(Gfx &d, const Game &game, const Chrome &chrome) {
  d.fillRect(0, gLayout.footerY, gLayout.w, kFooter, kHeaderBg);
  const char *names[5] = {"UNDO", "RESET", game.deathMatch() ? "DEATH ON" : "DEATHMATCH",
                          kModeName[chrome.colorMode], "BLACKOUT"};
  uint16_t fills[5] = {rgb(28, 32, 40), rgb(90, 24, 28),
                       game.deathMatch() ? rgb(90, 30, 140) : rgb(28, 32, 40), rgb(28, 32, 40),
                       rgb(28, 32, 40)};
  Rgb mode;
  scoreRgb(chrome.colorMode, 0, &mode);
  uint16_t inks[5] = {kWhite, kWhite, kWhite, rgb(mode.r, mode.g, mode.b), kYellow};
  for (int i = 0; i < 5; i++) {
    int x, y, w, h;
    buttonRect(i, &x, &y, &w, &h);
    d.fillRoundRect(x, y, w, h, 12, fills[i]);
    d.drawRoundRect(x, y, w, h, 12, rgb(70, 76, 88));
    label(d, &fonts::Font2, 1, inks[i], fills[i], middle_center, x + w / 2, y + h / 2, names[i]);
  }
}

template <class Gfx>
void drawServer(Gfx &d, const Game &game, int team, int cx, int top, int courtH, uint16_t panel) {
  if (game.deathMatch() || game.matchOver()) return;
  if (!game.serveChosen()) {
    label(d, &fonts::Font2, 1, kYellow, panel, middle_center, cx, top + courtH - 36, "TAP TO SERVE");
    return;
  }
  int server = 0, player = 0;
  game.currentServer(&server, &player);
  if (server != team) return;
  int x = team == 0 ? 28 : gLayout.w - 28;
  int y = player == 0 ? top + 36 : top + courtH / 2;
  d.fillCircle(x, y - 10, 8, kYellow);
  d.fillCircle(x, y + 10, 8, kYellow);
  label(d, &fonts::Font2, 1, kYellow, panel, middle_center, cx, top + courtH - 36,
        player == 0 ? "PLAYER 1 SERVES" : "PLAYER 2 SERVES");
}

template <class Gfx>
void drawLive(Gfx &d, const Game &game, const Chrome &chrome) {
  int top = kHeader;
  int courtH = gLayout.footerY - kHeader;
  int mid = gLayout.w / 2;
  bool death = game.deathMatch();
  int serveBand = (!death && !game.matchOver()) ? gFont2H + 36 : 16;
  int areaTop = top + 14;
  int areaBot = top + courtH - serveBand;
  if (areaBot < areaTop + gFont8H) areaBot = areaTop + gFont8H;

  int clockSize = 1;
  int clockW = 0;
  int clockH = 0;
  if (death) {
    clockSize = fitScale(gFont8H, areaBot - areaTop - gFont4H - 24, 2);
    int pairW = gFont8Digit * clockSize * 2;
    clockW = pairW * 2 + 72;
    clockH = gFont8H * clockSize + 36;
    if (clockW > mid - 80) {
      clockSize = 1;
      pairW = gFont8Digit * 2;
      clockW = pairW * 2 + 72;
      clockH = gFont8H + 36;
    }
  }

  for (int team = 0; team < 2; team++) {
    bool flash = chrome.phase == Phase::Flash && chrome.flashTeam == team;
    uint16_t panel = death ? (team == 0 ? kDeathL : kDeathR) : (team == 0 ? kBluePanel : kRedPanel);
    if (flash) panel = team == 0 ? kBlue : kRed;
    int x = team == 0 ? 0 : mid;
    d.fillRect(x, top, mid, courtH, panel);

    Rgb inkRgb;
    scoreRgb(chrome.colorMode, team, &inkRgb);
    uint16_t ink = flash ? kBlack : rgb(inkRgb.r, inkRgb.g, inkRgb.b);

    int colL = x + 24;
    int colR = x + mid - 24;
    if (death) {
      int clockL = mid - clockW / 2;
      int clockR = mid + clockW / 2;
      if (team == 0) colR = clockL - 16;
      else colL = clockR + 16;
    }
    int colW = colR - colL;
    if (colW < gFont8Digit) colW = gFont8Digit;
    int cx = (colL + colR) / 2;

    char hero[8];
    char sub[8];
    game.pointLabel(team, sub, sizeof(sub));
    bool showPoint = false;
    if (death || game.tiebreak()) {
      snprintf(hero, sizeof(hero), "%s", sub);
      sub[0] = '\0';
    } else {
      snprintf(hero, sizeof(hero), "%d", game.games(team, game.currentSet()));
      showPoint = sub[0] != '\0';
    }

    const char *banner = nullptr;
    uint16_t bannerInk = ink;
    if (game.tiebreak()) banner = "TIE-BREAK";
    else if (game.goldenPoint()) {
      banner = "GOLDEN POINT";
      bannerInk = kYellow;
    }

    int cursor = areaTop;
    int nameY = cursor + gFont4H / 2;
    label(d, &fonts::Font4, 1, ink, panel, middle_center, cx, nameY, team == 0 ? "BLUE" : "RED");
    cursor += gFont4H + 12;
    if (banner) {
      label(d, &fonts::Font2, 1, bannerInk, panel, middle_center, cx, cursor + gFont2H / 2, banner);
      cursor += gFont2H + 10;
    }

    int avail = areaBot - cursor;
    if (avail < gFont4H) avail = gFont4H;
    int gap = 20;
    int heroSize = fitScale(gFont8H, avail, 4);
    int digits = 1;
    for (const char *p = hero; *p; ++p)
      if (*p >= '0' && *p <= '9') digits++;
    digits--;
    if (digits < 1) digits = 1;
    while (heroSize > 1 && gFont8Digit * heroSize * digits > colW) heroSize--;

    int pointSize = 1;
    int pointH = 0;
    if (showPoint) {
      int pointFont = font8Safe(sub) ? gFont8H : gFont4H;
      pointSize = fitScale(pointFont, avail / 3, font8Safe(sub) ? 2 : 3);
      pointH = pointSize * pointFont;
      while (heroSize > 1 && heroSize * gFont8H + gap + pointH > avail) heroSize--;
      while (pointSize > 1 && heroSize * gFont8H + gap + pointSize * pointFont > avail) pointSize--;
      pointH = pointSize * pointFont;
    }
    int heroH = heroSize * gFont8H;
    int block = heroH + (showPoint ? gap + pointH : 0);
    if (block > avail) block = avail;
    int blockTop = cursor + (avail - block) / 2;
    if (blockTop < cursor) blockTop = cursor;
    drawBlock(d, hero, heroSize, ink, panel, cx, blockTop + heroH / 2);
    if (showPoint) {
      int pointY = blockTop + heroH + gap + pointH / 2;
      drawBlock(d, sub, pointSize, ink, panel, cx, pointY);
    }
    drawServer(d, game, team, cx, top, courtH, panel);
  }

  d.fillRect(mid - 3, top, 6, courtH, kNet);

  if (death) {
    int sec = game.clockSeconds();
    char ms[4], ss[4];
    snprintf(ms, sizeof(ms), "%02d", sec / 60);
    snprintf(ss, sizeof(ss), "%02d", sec % 60);
    int pairW = gFont8Digit * clockSize * 2;
    int bw = clockW;
    int bh = clockH;
    int bx = mid - bw / 2;
    int by = areaTop + gFont4H + 12;
    int avail = areaBot - by;
    if (bh < avail) by += (avail - bh) / 2;
    d.fillRoundRect(bx, by, bw, bh, 16, kBlack);
    d.drawRoundRect(bx, by, bw, bh, 16, kGreen);
    int cy = by + bh / 2;
    int half = pairW / 2 + 16;
    label(d, &fonts::Font8, clockSize, kWhite, kBlack, middle_center, mid - half, cy, ms);
    label(d, &fonts::Font8, clockSize, kWhite, kBlack, middle_center, mid + half, cy, ss);
    d.fillCircle(mid, cy - 16, 6, kGreen);
    d.fillCircle(mid, cy + 16, 6, kGreen);
  }
}

template <class Gfx>
void drawSummary(Gfx &d, const Game &game, const Chrome &chrome) {
  int top = kHeader;
  int courtH = gLayout.footerY - kHeader;
  uint16_t panel = chrome.summaryTeam == 0 ? kBluePanel : kRedPanel;
  d.fillRect(0, top, gLayout.w, courtH, panel);
  d.drawRect(10, top + 10, gLayout.w - 20, courtH - 20, kGreen);
  d.drawRect(14, top + 14, gLayout.w - 28, courtH - 28, kGreen);
  const char *who = chrome.summaryTeam == 0 ? "BLUE" : "RED";
  char title[24];
  snprintf(title, sizeof(title), "%s TAKES THE %s", who, chrome.summaryIsSet ? "SET" : "GAME");
  int titleY = top + 28 + gFont4H / 2;
  label(d, &fonts::Font4, 1, kWhite, panel, middle_center, gLayout.w / 2, titleY, title);

  int areaTop = titleY + gFont4H / 2 + 24;
  int areaBot = top + courtH - 36;
  int avail = areaBot - areaTop;
  if (avail < gFont8H * 2) avail = gFont8H * 2;
  int gap = 28;
  int rowSize = fitScale(gFont8H, (avail - gap) / 2, 2);
  int rowH = rowSize * gFont8H;
  int block = rowH * 2 + gap;
  int y0 = areaTop + (avail - block) / 2;
  int blueY = y0 + rowH / 2;
  int redY = y0 + rowH + gap + rowH / 2;
  int colGap = gFont8Digit * rowSize + 96;
  if (colGap < 180) colGap = 180;

  for (int set = 0; set < 3; set++) {
    int x = gLayout.w / 2 + (set - 1) * colGap;
    Rgb a, b;
    scoreRgb(chrome.colorMode, 0, &a);
    scoreRgb(chrome.colorMode, 1, &b);
    bool played = set <= game.currentSet();
    uint16_t ca = played ? rgb(a.r, a.g, a.b) : rgb(a.r / 3, a.g / 3, a.b / 3);
    uint16_t cb = played ? rgb(b.r, b.g, b.b) : rgb(b.r / 3, b.g / 3, b.b / 3);
    char left[4], right[4];
    snprintf(left, sizeof(left), "%d", game.games(0, set));
    snprintf(right, sizeof(right), "%d", game.games(1, set));
    label(d, &fonts::Font8, rowSize, ca, panel, middle_center, x, blueY, left);
    label(d, &fonts::Font8, rowSize, cb, panel, middle_center, x, redY, right);
    int mark = game.currentSet();
    if (chrome.summaryIsSet && mark > 0) mark -= 1;
    if (set == mark && !game.matchOver()) {
      d.fillRect(x - 24, redY + rowH / 2 + 10, 48, 6, kYellow);
    }
  }
  int labelX = gLayout.w / 2 - colGap - (gFont8Digit * rowSize) / 2 - 20;
  label(d, &fonts::Font4, 1, kBlue, panel, middle_right, labelX, blueY, "BLUE");
  label(d, &fonts::Font4, 1, kRed, panel, middle_right, labelX, redY, "RED");
}

template <class Gfx>
void drawEnds(Gfx &d) {
  int top = kHeader;
  int courtH = gLayout.footerY - kHeader;
  d.fillRect(0, top, gLayout.w, courtH, kYellow);
  d.setFont(&fonts::Font4);
  int titleSize = 5;
  while (titleSize > 1) {
    d.setTextSize(titleSize);
    if (d.textWidth("CHANGE ENDS") <= gLayout.w - 80 && titleSize * gFont4H <= courtH / 2) break;
    titleSize--;
  }
  int titleH = titleSize * gFont4H;
  int gap = 32;
  int block = titleH + gap + gFont4H;
  int y0 = top + (courtH - block) / 2;
  label(d, &fonts::Font4, titleSize, kBlack, kYellow, middle_center, gLayout.w / 2, y0 + titleH / 2,
        "CHANGE ENDS");
  label(d, &fonts::Font4, 1, kBlack, kYellow, middle_center, gLayout.w / 2, y0 + titleH + gap + gFont4H / 2,
        "ODD GAME");
}

template <class Gfx>
void drawWon(Gfx &d, const Game &game, const Chrome &chrome) {
  int top = kHeader;
  int courtH = gLayout.footerY - kHeader;
  int winner = game.winner();
  if (winner != 0 && winner != 1) winner = 0;
  uint16_t panel = winner == 0 ? kBluePanel : kRedPanel;
  d.fillRect(0, top, gLayout.w, courtH, panel);
  const char *title = winner == 0 ? "BLUE WINS" : "RED WINS";
  int last = game.currentSet();
  if (last < 0) last = 0;
  if (last > 2) last = 2;
  int rows = last + 1;
  int titleSize = 4;
  d.setFont(&fonts::Font4);
  while (titleSize > 1) {
    d.setTextSize(titleSize);
    if (d.textWidth(title) <= gLayout.w - 80) break;
    titleSize--;
  }
  int gap = 16;
  int rowSize = fitScale(gFont8H, (courtH - titleSize * gFont4H - 48 - gap * rows) / rows, 2);
  int titleH = titleSize * gFont4H;
  int rowH = rowSize * gFont8H;
  int y = top + 28;
  label(d, &fonts::Font4, titleSize, kWhite, panel, middle_center, gLayout.w / 2, y + titleH / 2, title);
  y += titleH + 28;
  for (int set = 0; set <= last; set++) {
    char row[16];
    snprintf(row, sizeof(row), "%d  -  %d", game.games(0, set), game.games(1, set));
    label(d, &fonts::Font8, rowSize, kWhite, panel, middle_center, gLayout.w / 2, y + rowH / 2, row);
    y += rowH + gap;
  }

  uint16_t win = winner == 0 ? kBlue : kRed;
  int phase = chrome.anim & 7;
  for (int x = 0; x < gLayout.w; x += 20) {
    uint16_t c = ((x / 20 + phase) & 1) ? win : kWhite;
    d.fillRect(x, top, 20, 8, c);
    d.fillRect(x, gLayout.footerY - 8, 20, 8, c);
  }
  for (int y = top; y < gLayout.footerY; y += 20) {
    uint16_t c = ((y / 20 + phase) & 1) ? win : kWhite;
    d.fillRect(0, y, 8, 20, c);
    d.fillRect(gLayout.w - 8, y, 8, 20, c);
  }
}

template <class Gfx>
void drawBlackout(Gfx &d, const Chrome &chrome) {
  d.fillScreen(kBlack);
  uint16_t c = chrome.hopGreen ? kGreen : kYellow;
  d.fillCircle(chrome.hopX, chrome.hopY, 16, c);
}

template <class Gfx>
void paint(Gfx &d, const Game &game, const Chrome &chrome) {
  if (chrome.blackout) {
    drawBlackout(d, chrome);
    return;
  }
  d.fillScreen(kHeaderBg);
  drawHeader(d, game, chrome);
  if (chrome.phase == Phase::Ends) drawEnds(d);
  else if (chrome.phase == Phase::Won) drawWon(d, game, chrome);
  else if (chrome.phase == Phase::Summary) drawSummary(d, game, chrome);
  else drawLive(d, game, chrome);
  drawFooter(d, game, chrome);
}

}  // namespace

int displayColorCount() { return 9; }

const char *displayColorName(int mode) {
  if (mode < 0 || mode >= displayColorCount()) return kModeName[0];
  return kModeName[mode];
}

void displayBegin() {
  gLayout.w = M5.Display.width();
  gLayout.h = M5.Display.height();
  if (gLayout.w < 100) gLayout.w = 1280;
  if (gLayout.h < 100) gLayout.h = 720;
  gLayout.footerY = gLayout.h - kFooter;

  kHeaderBg = rgb(8, 10, 14);
  kBluePanel = rgb(0, 32, 78);
  kRedPanel = rgb(78, 14, 22);
  kDeathL = rgb(42, 0, 72);
  kDeathR = rgb(78, 28, 0);
  kWhite = rgb(255, 255, 255);
  kBlack = rgb(0, 0, 0);
  kYellow = rgb(255, 204, 0);
  kGreen = rgb(0, 210, 70);
  kOrange = rgb(255, 120, 0);
  kNet = rgb(235, 235, 235);
  kDim = rgb(110, 116, 128);
  kBlue = rgb(0, 150, 255);
  kRed = rgb(255, 55, 65);

  static M5Canvas canvas(&M5.Display);
  gCanvas = &canvas;
  gCanvas->setPsram(true);
  gCanvas->setColorDepth(16);
  gSprite = gCanvas->createSprite(gLayout.w, gLayout.h) != nullptr;
  if (gSprite) measure(*gCanvas);
  else measure(M5.Display);
}

Hit displayHitTest(int x, int y) {
  if (y >= gLayout.footerY) {
    int slot = gLayout.w / 5;
    if (slot < 1) return Hit::None;
    int index = x / slot;
    if (index < 0 || index > 4) return Hit::None;
    static const Hit kHits[] = {Hit::Undo, Hit::Reset, Hit::Death, Hit::Color, Hit::Blackout};
    return kHits[index];
  }
  if (y >= kHeader && x >= 0 && x < gLayout.w) return x < gLayout.w / 2 ? Hit::Score0 : Hit::Score1;
  return Hit::None;
}

void displayRender(const Game &game, const Chrome &chrome) {
  if (gSprite && gCanvas) {
    paint(*gCanvas, game, chrome);
    gCanvas->pushSprite(0, 0);
  } else {
    paint(M5.Display, game, chrome);
  }
}
