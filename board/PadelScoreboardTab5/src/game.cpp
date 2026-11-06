#include "game.h"

#include <stdio.h>
#include <string.h>

namespace {

int clamp99(int value) {
  if (value < 0) return 0;
  if (value > 99) return 99;
  return value;
}

}  // namespace

void Game::resetMatch() {
  memset(points_, 0, sizeof(points_));
  deuceCount_ = 0;
  advantageTeam_ = -1;
  inTiebreak_ = false;
  memset(games_, 0, sizeof(games_));
  currentSet_ = 0;
  memset(sets_, 0, sizeof(sets_));
  matchOver_ = false;
  winner_ = -1;
  dmSeconds_ = 0;
  historyCount_ = 0;
  serveTeam_ = 0;
  servePlayer_[0] = 0;
  servePlayer_[1] = 0;
  serveChosen_ = false;
}

bool Game::toggleDeathmatch() {
  deathMatch_ = !deathMatch_;
  if (deathMatch_) {
    dmSeconds_ = 0;
    resetMatch();
  }
  return deathMatch_;
}

void Game::setClockSeconds(int seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds > 3600) seconds = 3600;
  dmSeconds_ = seconds;
}

bool Game::blank() const {
  if (deathMatch_ || inTiebreak_ || matchOver_ || currentSet_) return false;
  if (points_[0] || points_[1] || deuceCount_ || advantageTeam_ != -1) return false;
  if (sets_[0] || sets_[1]) return false;
  for (int set = 0; set < 3; set++) {
    if (games_[0][set] || games_[1][set]) return false;
  }
  return true;
}

void Game::capture(Snapshot *s) const {
  memcpy(s->points, points_, sizeof(points_));
  s->deuceCount = deuceCount_;
  s->advantageTeam = advantageTeam_;
  s->inTiebreak = inTiebreak_;
  memcpy(s->games, games_, sizeof(games_));
  s->currentSet = currentSet_;
  memcpy(s->sets, sets_, sizeof(sets_));
  s->matchOver = matchOver_;
  s->winner = winner_;
  s->deathMatch = deathMatch_;
  s->dmSeconds = dmSeconds_;
  s->serveTeam = serveTeam_;
  s->servePlayer[0] = servePlayer_[0];
  s->servePlayer[1] = servePlayer_[1];
  s->serveChosen = serveChosen_;
}

void Game::restore(const Snapshot *s) {
  memcpy(points_, s->points, sizeof(points_));
  deuceCount_ = s->deuceCount;
  advantageTeam_ = s->advantageTeam;
  inTiebreak_ = s->inTiebreak;
  memcpy(games_, s->games, sizeof(games_));
  currentSet_ = s->currentSet;
  memcpy(sets_, s->sets, sizeof(sets_));
  matchOver_ = s->matchOver;
  winner_ = s->winner;
  deathMatch_ = s->deathMatch;
  dmSeconds_ = s->dmSeconds;
  serveTeam_ = s->serveTeam;
  servePlayer_[0] = s->servePlayer[0];
  servePlayer_[1] = s->servePlayer[1];
  serveChosen_ = s->serveChosen;
}

void Game::push() {
  if (historyCount_ == kHistory) {
    memmove(&history_[0], &history_[1], sizeof(Snapshot) * (kHistory - 1));
    historyCount_ = kHistory - 1;
  }
  capture(&history_[historyCount_++]);
}

bool Game::undo() {
  if (historyCount_ <= 0) return false;
  restore(&history_[--historyCount_]);
  return true;
}

void Game::advanceServe() {
  servePlayer_[serveTeam_] ^= 1;
  serveTeam_ ^= 1;
}

void Game::currentServer(int *teamOut, int *playerOut) const {
  int p0 = servePlayer_[0];
  int p1 = servePlayer_[1];
  int t = serveTeam_;
  if (!inTiebreak_) {
    *teamOut = t;
    *playerOut = (t == 0) ? p0 : p1;
    return;
  }
  int played = points_[0] + points_[1];
  if (played <= 0) {
    *teamOut = t;
    *playerOut = (t == 0) ? p0 : p1;
    return;
  }
  played -= 1;
  if (t == 0) p0 ^= 1;
  else p1 ^= 1;
  t ^= 1;
  while (played >= 2) {
    played -= 2;
    if (t == 0) p0 ^= 1;
    else p1 ^= 1;
    t ^= 1;
  }
  *teamOut = t;
  *playerOut = (t == 0) ? p0 : p1;
}

bool Game::goldenPoint() const {
  return !deathMatch_ && !inTiebreak_ && advantageTeam_ == -1 && deuceCount_ >= 2 &&
         points_[0] >= 3 && points_[1] >= 3;
}

static uint8_t clampByte(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (uint8_t)value;
}

void Game::exportView(ScoreView *view) const {
  memset(view, 0, sizeof(*view));
  view->version = SCORE_LINK_VERSION;
  view->points[0] = clampByte(points_[0]);
  view->points[1] = clampByte(points_[1]);
  view->deuceCount = clampByte(deuceCount_);
  view->advantageTeam = (int8_t)advantageTeam_;
  if (inTiebreak_) view->flags |= SCORE_F_TIEBREAK;
  if (deathMatch_) view->flags |= SCORE_F_DEATH;
  if (matchOver_) view->flags |= SCORE_F_MATCH;
  if (serveChosen_) view->flags |= SCORE_F_SERVE;
  view->currentSet = (uint8_t)currentSet_;
  for (int team = 0; team < 2; team++) {
    view->setsWon[team] = clampByte(sets_[team]);
    for (int set = 0; set < 3; set++) view->games[team][set] = clampByte(games_[team][set]);
  }
  view->serveTeam = (uint8_t)serveTeam_;
  view->servePlayer[0] = (uint8_t)servePlayer_[0];
  view->servePlayer[1] = (uint8_t)servePlayer_[1];
  view->winner = (int8_t)winner_;
  view->clockSeconds = (uint16_t)dmSeconds_;
}

void Game::applyView(const ScoreView &view) {
  points_[0] = view.points[0];
  points_[1] = view.points[1];
  deuceCount_ = view.deuceCount;
  advantageTeam_ = view.advantageTeam;
  inTiebreak_ = (view.flags & SCORE_F_TIEBREAK) != 0;
  deathMatch_ = (view.flags & SCORE_F_DEATH) != 0;
  matchOver_ = (view.flags & SCORE_F_MATCH) != 0;
  serveChosen_ = (view.flags & SCORE_F_SERVE) != 0;
  currentSet_ = view.currentSet > 2 ? 2 : view.currentSet;
  for (int team = 0; team < 2; team++) {
    sets_[team] = view.setsWon[team];
    for (int set = 0; set < 3; set++) games_[team][set] = view.games[team][set];
  }
  serveTeam_ = view.serveTeam & 1;
  servePlayer_[0] = view.servePlayer[0];
  servePlayer_[1] = view.servePlayer[1];
  winner_ = view.winner;
  dmSeconds_ = view.clockSeconds > 3600 ? 3600 : view.clockSeconds;
  historyCount_ = 0;
}

void Game::pointLabel(int team, char *out, size_t n) const {
  team &= 1;
  if (n == 0) return;
  if (deathMatch_ || inTiebreak_) {
    snprintf(out, n, "%d", points_[team]);
    return;
  }
  if (points_[team] < 3) {
    static const char *k[] = {"0", "15", "30"};
    snprintf(out, n, "%s", k[points_[team]]);
    return;
  }
  if (advantageTeam_ == -1 && deuceCount_ >= 2) {
    snprintf(out, n, "SP");
    return;
  }
  if (advantageTeam_ == team) {
    snprintf(out, n, "AD");
    return;
  }
  snprintf(out, n, "40");
}

void Game::onGameWon(int team, ScoreResult *result) {
  if (deathMatch_) return;
  bool wasTiebreak = inTiebreak_;
  points_[0] = points_[1] = 0;
  deuceCount_ = 0;
  advantageTeam_ = -1;
  games_[team][currentSet_]++;
  int total = games_[0][currentSet_] + games_[1][currentSet_];
  result->changeEnds = (total % 2) == 1;

  if (wasTiebreak) {
    // The matrix firmware asked the game count for a two-game margin. At 6-6
    // the tie-break winner is 7-6, so that test never passed and the set stayed
    // open. Winning the tie-break wins the set. The set is recorded as 7-6
    // (or 7-6 after a longer tie-break, still one game).
    inTiebreak_ = false;
    sets_[team]++;
    if (sets_[team] >= 2) {
      matchOver_ = true;
      winner_ = team;
      result->effect = ScoreEffect::MatchWon;
    } else {
      if (currentSet_ < 2) currentSet_++;
      result->effect = ScoreEffect::SetWon;
    }
  } else if (games_[team][currentSet_] >= 6 &&
             games_[team][currentSet_] - games_[1 - team][currentSet_] >= 2) {
    sets_[team]++;
    if (sets_[team] >= 2) {
      matchOver_ = true;
      winner_ = team;
      result->effect = ScoreEffect::MatchWon;
    } else {
      if (currentSet_ < 2) currentSet_++;
      result->effect = ScoreEffect::SetWon;
    }
  } else if (games_[0][currentSet_] == 6 && games_[1][currentSet_] == 6) {
    inTiebreak_ = true;
    result->effect = ScoreEffect::Game;
  } else {
    result->effect = ScoreEffect::Game;
  }
  advanceServe();
  if (matchOver_) result->changeEnds = false;
}

ScoreResult Game::score(int team) {
  ScoreResult result{ScoreEffect::Ignored, false, team};
  if (team < 0 || team > 1 || matchOver_) return result;

  // At 0-0 the first score press picks that side's player 1. It is not a point.
  if (!serveChosen_ && blank()) {
    push();
    serveTeam_ = team;
    servePlayer_[0] = 0;
    servePlayer_[1] = 0;
    serveChosen_ = true;
    result.effect = ScoreEffect::ServeChosen;
    return result;
  }

  push();
  bool wonGame = false;
  if (deathMatch_) {
    points_[team] = clamp99(points_[team] + 1);
  } else {
    int other = 1 - team;
    if (inTiebreak_) {
      points_[team]++;
      if (points_[team] >= 7 && points_[team] - points_[other] >= 2) wonGame = true;
    } else if (points_[team] < 3) {
      points_[team]++;
    } else if (points_[other] < 3) {
      wonGame = true;
    } else if (advantageTeam_ == -1) {
      if (deuceCount_ >= 2) wonGame = true;
      else advantageTeam_ = team;
    } else if (advantageTeam_ == team) {
      wonGame = true;
    } else {
      advantageTeam_ = -1;
      deuceCount_++;
    }
  }

  if (wonGame) onGameWon(team, &result);
  else result.effect = ScoreEffect::Point;
  return result;
}
