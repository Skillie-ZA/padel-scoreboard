#pragma once

#include "game.h"

#include <stdint.h>

enum class Phase : uint8_t { Live, Flash, Summary, Ends, Won };

enum class Hit : uint8_t { None, Score0, Score1, Undo, Reset, Death, Color, Blackout };

struct Chrome {
  Phase phase;
  int colorMode;
  bool blackout;
  int hopX;
  int hopY;
  bool hopGreen;
  int boardBattery;
  int remoteBattery[2];
  bool remoteSleep[2];
  bool remoteStale[2];
  const char *mac;
  int channel;
  bool radioOk;
  int flashTeam;
  int anim;
  bool summaryIsSet;
  int summaryTeam;
};

int displayColorCount();
const char *displayColorName(int mode);

void displayBegin();
Hit displayHitTest(int x, int y);
void displayRender(const Game &game, const Chrome &chrome);
