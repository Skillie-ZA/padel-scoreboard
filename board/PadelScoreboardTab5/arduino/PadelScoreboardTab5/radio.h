#pragma once

#include "score_link.h"

#include <stddef.h>
#include <stdint.h>

// Brings up the SDIO link to the C6. The match itself runs on the C6.
// macOut receives the C6 STA MAC as "AA:BB:CC:DD:EE:FF".
bool radioBegin(char *macOut, size_t macLen, int *channelOut);

// True once a snapshot has arrived in the last few seconds.
bool radioLinked();

bool radioPoll(ScoreView *out);
bool radioSend(uint8_t cmd, uint8_t team, uint8_t data);

// Serial command "C6OTA <bytes>" streams a slave app image to the C6.
bool radioOtaActive();
void radioOtaBegin(uint32_t size);
void radioOtaPump();
