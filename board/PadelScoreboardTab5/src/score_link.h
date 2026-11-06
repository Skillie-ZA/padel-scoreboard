#pragma once

#include <stdint.h>

// Snapshot the C6 scoreboard sends to the P4, and the command the P4 sends back.
// Both chips are little-endian. uint16 fields sit on even offsets so a packed
// copy is a normal aligned load when the struct itself is aligned.
// Layout is shared. Do not reorder it.

#define SCORE_LINK_VERSION 1u
#define SCORE_MSG_CMD 0x50310001u
#define SCORE_MSG_VIEW 0x50310002u

#define SCORE_F_TIEBREAK 0x01u
#define SCORE_F_DEATH 0x02u
#define SCORE_F_MATCH 0x04u
#define SCORE_F_SERVE 0x08u
#define SCORE_F_FORCE_LIVE 0x10u

struct ScoreCmd {
  uint8_t cmd;
  uint8_t team;
  uint8_t data;
} __attribute__((packed));

struct ScoreView {
  uint8_t version;
  uint8_t effect;
  uint8_t effectTeam;
  uint8_t changeEnds;
  uint16_t seq;
  uint8_t points[2];
  uint8_t deuceCount;
  int8_t advantageTeam;
  uint8_t flags;
  uint8_t currentSet;
  uint8_t games[2][3];
  uint8_t setsWon[2];
  uint8_t serveTeam;
  uint8_t servePlayer[2];
  int8_t winner;
  uint16_t clockSeconds;
  int8_t remotePct[2];
  uint8_t remoteSleep[2];
  uint8_t channel;
  uint8_t reserved;
} __attribute__((packed));

static_assert(sizeof(ScoreCmd) == 3, "touch command is the 3-byte remote packet");
static_assert(sizeof(ScoreView) == 32, "score snapshot size drifted");
