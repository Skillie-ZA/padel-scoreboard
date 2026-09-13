# Padel Scoreboard

ESP-NOW padel scoreboard and handheld remote.

Current release: **v1.1**. v1.0 remains in the tree for reference.

## What’s in this repo

| Version | Part | Board | Sketch |
|---|---|---|---|
| **v1.1** | Scoreboard | ESP32-C6 | `board/ScoreBoard_V1_1-Board/` |
| **v1.1** | Remote (one per team) | ESP32-C6 | `remote/Scoreboard_V1_1-Remote/` |
| v1.0 | Scoreboard | ESP32-C3 | `board/ScoreBoard_V1_Final-Board/` |
| v1.0 | Remote (one for both teams) | ESP32-C6 | `remote/Scoreboard_Final_V1-Remote/` |

The sketches talk over ESP-NOW. No Wi-Fi router is required. v1.1 remotes are hardcoded to the scoreboard MAC printed on Serial at boot.

## Hardware (v1.1)

**Scoreboard (ESP32-C6)**
- 16×16 WS2812B matrix on **GPIO 6** (256 LEDs)
- Mode button / wake on **GPIO 7**
- LED driver enable on **GPIO 23** (driven LOW in sleep)
- Battery ADC on **GPIO 0**

**Remote (ESP32-C6, one unit per team)**
- WS2812 LED on **GPIO 7**
- Button 1 on **GPIO 6** (this team: score / reset / sleep, and **wake**)
- Button 2 on **GPIO 20** (undo / deathmatch / other-team mode)
- Battery ADC on **GPIO 0**

ESP32-C6 can only wake from deep sleep on GPIO 0–7. Button 1 (GPIO 6) is the wake button. Button 2 (GPIO 20) cannot wake the chip.

## Scoreboard controls (v1.1)

| Board button | Action |
|---|---|
| Short (&lt; 1 s) | Cycle score colours |
| 1–3 s | Toggle deathmatch |
| 3 s+ | Sleep (wake on GPIO 7) |

Colour cycle:

1. **Both white** (default, high visibility)
2. **Blue / red** team colours
3. Shared orange, yellow, green, cyan, blue, purple, red

A scored point flashes the whole matrix in that team’s blue or red for 20 ms, then returns to the current screen.

On boot the scoreboard prints its MAC on Serial, e.g.

```text
MAC: FC:01:2C:EC:3F:48
uint8_t scoreboardAddress[] = {0xFC, 0x01, 0x2C, 0xEC, 0x3F, 0x48};
```

Paste that array into the remote sketch.

## Remote controls (v1.1)

Set `#define TEAM 1` (blue) or `#define TEAM 2` (red) before flashing each remote.

| Press | Button 1 | Button 2 |
|---|---|---|
| 0.5–2 s | — | Undo last point, or score the **other** team if Other Team Mode is on |
| &lt; 2 s | Score this team | — |
| 2–4 s | Reset match | — |
| 4 s+ | Sleep (LED yellow first) | — |
| 5–7 s | — | Toggle deathmatch |
| 10 s+ | — | Toggle Other Team Mode |

LED: team-colour breathe every 5 seconds, **green** on a successful send, yellow before sleep. After 20 minutes with no presses it sleeps. Press **Button 1** to wake.

## ESP-NOW message (v1.1)

```text
cmd:  0 = score, 1 = undo, 99 = reset, 100 = battery, 101 = sleep, 102 = deathmatch
team: 1 or 2
data: battery percent (cmd 100) or unused
```

## Flash with Arduino IDE / arduino-cli

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and the **esp32** board package.
2. Install the **FastLED** library.
3. Open the sketch folder (folder name must match the `.ino` name).
4. Select **ESP32C6 Dev Module**, enable **USB CDC On Boot**, then Upload.

Command-line, scoreboard:

```bash
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32c6:CDCOnBoot=cdc,UploadSpeed=921600" \
  board/ScoreBoard_V1_1-Board
```

Remote (change `TEAM` in the sketch for the red unit):

```bash
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32c6:CDCOnBoot=cdc,UploadSpeed=921600" \
  remote/Scoreboard_V1_1-Remote
```

## Versioning

| Tag | Notes |
|---|---|
| **v1.0** | C3 scoreboard + one two-button C6 remote for both teams |
| **v1.1** | C6 V2 scoreboard: white-first colours, 20 ms team flash, undo/deathmatch from remote, MAC on Serial; per-team remotes with v1.1 buttons, 5 s breathe, GPIO wake |
