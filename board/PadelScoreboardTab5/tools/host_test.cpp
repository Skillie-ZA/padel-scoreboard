#include "espnow_frame.h"
#include "game.h"
#include "score_link.h"

#include <stdio.h>
#include <string.h>

static int gFailed = 0;

static void check(bool ok, const char *what) {
  if (ok) return;
  printf("FAIL %s\n", what);
  gFailed++;
}

static ScoreEffect winLove(Game *g, int team) {
  ScoreEffect last = ScoreEffect::Ignored;
  for (int i = 0; i < 4; i++) last = g->score(team).effect;
  return last;
}

static void expectLabel(Game *g, int team, const char *want, const char *what) {
  char buf[8];
  g->pointLabel(team, buf, sizeof(buf));
  if (strcmp(buf, want) != 0) {
    printf("FAIL %s (got %s, want %s)\n", what, buf, want);
    gFailed++;
  }
}

static void playToSixSix(Game *g) {
  for (int i = 0; i < 6; i++) {
    check(winLove(g, 0) == ScoreEffect::Game, "blue game toward 6-6");
    check(winLove(g, 1) == (i == 5 ? ScoreEffect::Game : ScoreEffect::Game), "red game toward 6-6");
  }
  check(g->games(0, 0) == 6 && g->games(1, 0) == 6, "games are 6-6");
  check(g->tiebreak(), "6-6 starts a tie-break");
  check(g->currentSet() == 0, "tie-break stays in the set");
}

int main() {
  Game g;
  ScoreResult r = g.score(0);
  check(r.effect == ScoreEffect::ServeChosen, "first press chooses the server");
  check(g.points(0) == 0 && g.points(1) == 0, "choosing the server is not a point");
  int team = -1, player = -1;
  g.currentServer(&team, &player);
  check(team == 0 && player == 0, "chosen side starts on player 1");

  check(g.undo(), "undo the serve choice");
  check(!g.serveChosen(), "serve is unchosen after undo");
  g.score(0);

  expectLabel(&g, 0, "0", "love");
  g.score(0);
  expectLabel(&g, 0, "15", "15");
  g.score(0);
  expectLabel(&g, 0, "30", "30");
  g.score(0);
  expectLabel(&g, 0, "40", "40");
  r = g.score(0);
  check(r.effect == ScoreEffect::Game && r.changeEnds, "love game, change ends");
  check(g.games(0, 0) == 1 && g.points(0) == 0, "game recorded and points cleared");
  g.currentServer(&team, &player);
  check(team == 1 && player == 0, "serve crosses after the game");

  check(winLove(&g, 1) == ScoreEffect::Game, "second game");
  check(g.games(0, 0) == 1 && g.games(1, 0) == 1, "one game each");
  g.currentServer(&team, &player);
  check(team == 0 && player == 1, "each side's second player comes around");

  // Fresh match for deuce, golden point, sets, and the tie-break.
  g.resetMatch();
  g.score(0);

  // 40-40, two advantages lost, then sudden point.
  for (int i = 0; i < 3; i++) g.score(0);
  for (int i = 0; i < 3; i++) g.score(1);
  expectLabel(&g, 0, "40", "deuce shows 40");
  expectLabel(&g, 1, "40", "deuce shows 40 for both");
  g.score(0);
  expectLabel(&g, 0, "AD", "advantage");
  expectLabel(&g, 1, "40", "other side stays on 40");
  g.score(1);
  expectLabel(&g, 0, "40", "back to deuce");
  g.score(0);
  g.score(1);
  check(g.goldenPoint(), "golden point after two deuces");
  expectLabel(&g, 0, "SP", "sudden point label");
  expectLabel(&g, 1, "SP", "sudden point for both");
  r = g.score(1);
  check(r.effect == ScoreEffect::Game, "sudden point wins the game");
  check(g.games(1, 0) == 1, "red took the game");

  g.resetMatch();
  g.score(0);
  for (int i = 0; i < 5; i++) check(winLove(&g, 0) == ScoreEffect::Game, "game in a 6-0 set");
  check(winLove(&g, 0) == ScoreEffect::SetWon, "6-0 wins the set");
  check(g.setsWon(0) == 1 && g.currentSet() == 1, "set 2 is next");
  check(g.games(0, 0) == 6 && g.games(0, 1) == 0, "finished set kept, new set empty");

  // 7-5: five each, then the leader needs two.
  g.resetMatch();
  g.score(1);
  for (int i = 0; i < 5; i++) {
    check(winLove(&g, 0) == ScoreEffect::Game, "blue game to 5");
    check(winLove(&g, 1) == ScoreEffect::Game, "red game to 5");
  }
  check(winLove(&g, 0) == ScoreEffect::Game, "6-5 is not a set");
  check(g.currentSet() == 0 && !g.tiebreak(), "still the first set");
  check(winLove(&g, 0) == ScoreEffect::SetWon, "7-5 wins the set");
  check(g.games(0, 0) == 7 && g.games(1, 0) == 5, "set score 7-5");

  // 6-6 then a 7-0 tie-break closes the set at 7-6.
  g.resetMatch();
  g.score(0);
  playToSixSix(&g);
  g.currentServer(&team, &player);
  check(team == 0 && player == 0, "tie-break opens on the due server");
  r = g.score(0);
  check(r.effect == ScoreEffect::Point && g.points(0) == 1, "first tie-break point");
  g.currentServer(&team, &player);
  check(team == 1 && player == 0, "after one point the next player serves");
  g.score(1);
  g.currentServer(&team, &player);
  check(team == 1 && player == 0, "second point of the block, same server");
  g.score(0);
  g.currentServer(&team, &player);
  check(team == 0 && player == 1, "next block is the other team's second player");

  g.resetMatch();
  g.score(0);
  playToSixSix(&g);
  ScoreEffect last = ScoreEffect::Ignored;
  for (int i = 0; i < 7; i++) last = g.score(0).effect;
  check(last == ScoreEffect::SetWon, "7-0 tie-break wins the set");
  check(!g.tiebreak(), "tie-break is over");
  check(g.games(0, 0) == 7 && g.games(1, 0) == 6, "set is 7-6");
  check(g.setsWon(0) == 1 && g.currentSet() == 1 && g.points(0) == 0, "next set is ready");

  // Must win the tie-break by 2.
  g.resetMatch();
  g.score(0);
  playToSixSix(&g);
  for (int i = 0; i < 6; i++) {
    g.score(0);
    g.score(1);
  }
  check(g.tiebreak() && g.points(0) == 6 && g.points(1) == 6, "6-6 in the tie-break");
  check(g.score(0).effect == ScoreEffect::Point, "7-6 does not win");
  r = g.score(0);
  check(r.effect == ScoreEffect::SetWon, "8-6 wins the tie-break");
  check(g.games(0, 0) == 7 && g.games(1, 0) == 6, "a long tie-break is still one game");

  // Best of three. Split sets, then blue takes the decider.
  g.resetMatch();
  g.score(0);
  for (int set = 0; set < 2; set++) {
    int side = set;
    for (int i = 0; i < 5; i++) winLove(&g, side);
    check(winLove(&g, side) == ScoreEffect::SetWon, "set in a split match");
  }
  check(g.setsWon(0) == 1 && g.setsWon(1) == 1 && g.currentSet() == 2, "decider");
  for (int i = 0; i < 5; i++) winLove(&g, 0);
  check(winLove(&g, 0) == ScoreEffect::MatchWon, "third set wins the match");
  check(g.matchOver() && g.winner() == 0, "blue won 2-1");
  check(g.setsWon(0) == 2 && g.setsWon(1) == 1, "set tally 2-1");
  check(g.score(1).effect == ScoreEffect::Ignored, "no points after the match");

  // Undo restores the rally before the winning point, including the serve.
  g.resetMatch();
  g.score(0);
  g.score(0);
  g.score(0);
  g.score(0);
  check(g.points(0) == 3, "40");
  check(g.score(0).effect == ScoreEffect::Game, "game point");
  check(g.undo(), "undo the game");
  check(g.games(0, 0) == 0 && g.points(0) == 3, "back to 40");
  g.currentServer(&team, &player);
  check(team == 0 && player == 0, "serve restored with the undo");

  // Deathmatch counts rallies and does not play games. Turning it on clears the match.
  g.score(0);
  check(g.toggleDeathmatch(), "deathmatch on");
  check(g.points(0) == 0 && g.games(0, 0) == 0 && !g.serveChosen(), "deathmatch reset the match");
  check(g.score(0).effect == ScoreEffect::Point && g.points(0) == 1, "deathmatch point counts");
  check(g.score(1).effect == ScoreEffect::Point && g.points(1) == 1, "other side counts");
  for (int i = 0; i < 97; i++) g.score(0);
  check(g.points(0) == 98, "one rally short of the cap");
  check(g.undo(), "undo inside deathmatch");
  check(g.points(0) == 97, "undo removes one rally");
  g.score(0);
  g.score(0);
  check(g.points(0) == 99, "deathmatch stops at 99");
  check(g.score(0).effect == ScoreEffect::Point && g.points(0) == 99, "further rallies stay at 99");
  g.setClockSeconds(4000);
  check(g.clockSeconds() == 3600, "clock stops at 60 minutes");

  g.resetMatch();
  check(g.deathMatch(), "reset keeps deathmatch mode");
  check(g.points(0) == 0, "reset clears the rallies");
  check(!g.toggleDeathmatch(), "deathmatch off");
  check(g.points(0) == 0, "turning deathmatch off keeps the cleared score");

  Game src;
  src.score(0);
  src.score(0);
  src.score(0);
  ScoreView view;
  src.exportView(&view);
  check(sizeof(ScoreView) == 32, "snapshot is 32 bytes");
  check(view.version == SCORE_LINK_VERSION, "snapshot version");
  check((view.flags & SCORE_F_SERVE) != 0, "snapshot records the serve");
  Game dst;
  dst.applyView(view);
  expectLabel(&dst, 0, "30", "snapshot keeps the rally");
  dst.currentServer(&team, &player);
  check(team == 0 && player == 0, "snapshot keeps the server");
  check(dst.serveChosen() && !dst.deathMatch(), "snapshot keeps serve and mode");

  src.toggleDeathmatch();
  src.setClockSeconds(125);
  src.score(0);
  src.exportView(&view);
  dst.applyView(view);
  check(dst.deathMatch() && dst.clockSeconds() == 125 && dst.points(0) == 1,
        "snapshot keeps deathmatch");
  check(!dst.undo(), "the tablet copy has no undo history");

  const uint8_t self[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const uint8_t other[6] = {0x10, 0xBD, 0xA3, 0xA6, 0x46, 0x94};
  const uint8_t stranger[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  uint8_t frame[42];
  memset(frame, 0, sizeof(frame));
  frame[0] = 0xD0;
  frame[1] = 0x08;
  memcpy(frame + 4, self, 6);
  memcpy(frame + 10, other, 6);
  frame[22] = 0x30;
  frame[23] = 0x12;
  frame[24] = 0x7F;
  frame[25] = 0x18;
  frame[26] = 0xFE;
  frame[27] = 0x34;
  frame[32] = 0xDD;
  frame[33] = 8;
  frame[34] = 0x18;
  frame[35] = 0xFE;
  frame[36] = 0x34;
  frame[37] = 0x04;
  frame[38] = 0x11;
  frame[39] = 0;
  frame[40] = 2;
  frame[41] = 0;
  EspNowDatagram got;
  check(espnowParse(frame, sizeof(frame), self, &got), "remote action frame parses");
  check(got.cmd == 0 && got.team == 2 && got.data == 0, "score packet is the 3 body bytes");
  check(got.seq == 0x123, "sequence ignores the fragment nibble");
  check(memcmp(got.src, other, 6) == 0, "source mac is address 2");
  check(espnowCommandOk(got.cmd, got.team), "score from team 2 is a remote command");
  uint8_t broadcast[6];
  memset(broadcast, 0xFF, sizeof(broadcast));
  memcpy(frame + 4, broadcast, 6);
  check(espnowParse(frame, sizeof(frame), self, &got), "broadcast dest is accepted");
  memcpy(frame + 4, stranger, 6);
  check(!espnowParse(frame, sizeof(frame), self, &got), "another scoreboard's frame is ignored");
  memcpy(frame + 4, self, 6);
  frame[37] = 0x05;
  check(!espnowParse(frame, sizeof(frame), self, &got), "encrypted type is ignored");
  frame[37] = 0x04;
  frame[40] = 9;
  check(espnowParse(frame, sizeof(frame), self, &got), "bad team still has the frame shape");
  check(!espnowCommandOk(got.cmd, got.team), "team 9 is not a remote");
  frame[1] = 0x01;
  check(!espnowParse(frame, sizeof(frame), self, &got), "to-ds frame is not the remote layout");

  if (gFailed) {
    printf("%d failed\n", gFailed);
    return 1;
  }
  printf("ok\n");
  return 0;
}
