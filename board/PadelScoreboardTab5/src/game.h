#pragma once

#include "score_link.h"

#include <stddef.h>
#include <stdint.h>

// Padel rules from the v1.1 16x16 scoreboard, including the v1.1 remotes.
// Team 0 is blue (remote TEAM 1). Team 1 is red (remote TEAM 2).

enum class ScoreEffect : uint8_t {
  Ignored = 0,
  ServeChosen,
  Point,
  Game,
  SetWon,
  MatchWon,
};

struct ScoreResult {
  ScoreEffect effect;
  bool changeEnds;
  int team;
};

class Game {
 public:
  void resetMatch();
  bool toggleDeathmatch();
  ScoreResult score(int team);
  bool undo();

  void setClockSeconds(int seconds);
  int clockSeconds() const { return dmSeconds_; }

  bool deathMatch() const { return deathMatch_; }
  bool matchOver() const { return matchOver_; }
  int winner() const { return winner_; }
  bool tiebreak() const { return inTiebreak_; }
  bool serveChosen() const { return serveChosen_; }
  bool goldenPoint() const;

  int points(int team) const { return points_[team & 1]; }
  int games(int team, int set) const { return games_[team & 1][set]; }
  int setsWon(int team) const { return sets_[team & 1]; }
  int currentSet() const { return currentSet_; }

  void pointLabel(int team, char *out, size_t n) const;
  void currentServer(int *teamOut, int *playerOut) const;

  // Visible state only. Undo history stays on the chip that owns the match.
  void exportView(ScoreView *view) const;
  void applyView(const ScoreView &view);

 private:
  struct Snapshot {
    int points[2];
    int deuceCount;
    int advantageTeam;
    bool inTiebreak;
    int games[2][3];
    int currentSet;
    int sets[2];
    bool matchOver;
    int winner;
    bool deathMatch;
    int dmSeconds;
    int serveTeam;
    int servePlayer[2];
    bool serveChosen;
  };

  bool blank() const;
  void push();
  void capture(Snapshot *s) const;
  void restore(const Snapshot *s);
  void advanceServe();
  void onGameWon(int team, ScoreResult *result);

  int points_[2] = {0, 0};
  int deuceCount_ = 0;
  int advantageTeam_ = -1;
  bool inTiebreak_ = false;
  int games_[2][3] = {{0, 0, 0}, {0, 0, 0}};
  int currentSet_ = 0;
  int sets_[2] = {0, 0};
  bool matchOver_ = false;
  int winner_ = -1;
  bool deathMatch_ = false;
  int dmSeconds_ = 0;
  int serveTeam_ = 0;
  int servePlayer_[2] = {0, 0};
  bool serveChosen_ = false;

  static constexpr int kHistory = 16;
  Snapshot history_[kHistory];
  int historyCount_ = 0;
};
