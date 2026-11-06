#pragma once

// Unencrypted ESP-NOW v1 and v2, as a 3-address 802.11 action frame with the
// FCS already removed. v1 and v2 use the same header size; the version nibble
// lives in the byte before the body. The remote sends exactly 3 bytes.

#include <stdint.h>
#include <string.h>

struct EspNowDatagram {
  uint8_t cmd;
  uint8_t team;
  uint8_t data;
  uint8_t src[6];
  uint16_t seq;
};

inline bool espnowCommandOk(uint8_t cmd, uint8_t team) {
  if (team != 1 && team != 2) return false;
  switch (cmd) {
    case 0:
    case 1:
    case 99:
    case 100:
    case 101:
    case 102:
      return true;
    default:
      return false;
  }
}

// self is the scoreboard STA MAC. A broadcast destination is accepted too.
inline bool espnowParse(const uint8_t *frame, unsigned len, const uint8_t self[6],
                        EspNowDatagram *out) {
  if (!frame || !self || !out || len < 42 || len > 300) return false;
  if (frame[0] != 0xD0) return false;
  if ((frame[1] & 0x03) != 0) return false;
  bool broadcast = true;
  for (int i = 0; i < 6; i++) {
    if (frame[4 + i] != 0xFF) broadcast = false;
  }
  if (!broadcast && memcmp(frame + 4, self, 6) != 0) return false;
  if (frame[24] != 0x7F) return false;
  if (frame[25] != 0x18 || frame[26] != 0xFE || frame[27] != 0x34) return false;
  if (frame[32] != 0xDD) return false;
  unsigned elen = frame[33];
  if (elen < 5 || 34u + elen > len) return false;
  if (frame[34] != 0x18 || frame[35] != 0xFE || frame[36] != 0x34) return false;
  if (frame[37] != 0x04) return false;
  if (elen - 5 != 3) return false;
  out->cmd = frame[39];
  out->team = frame[40];
  out->data = frame[41];
  memcpy(out->src, frame + 10, 6);
  uint16_t seqCtrl = (uint16_t)frame[22] | ((uint16_t)frame[23] << 8);
  out->seq = (uint16_t)(seqCtrl >> 4);
  return true;
}
