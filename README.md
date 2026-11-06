# Padel Scoreboard

ESP-NOW padel scoreboard and handheld remote.

Current release: **v1.2** (C3 16×16 / wide 8×32, with serve dots also back-ported to the C6 v1.1 board). v2.0 interim, Tab5, v1.1 remotes, and v1.0 remain in the tree.

## What’s in this repo

| Version | Part | Board | Sketch |
|---|---|---|---|
| **v1.2** | 16×16 scoreboard | ESP32-C3 | `board/ScoreBoard_16x16_C3/` |
| **v1.2** | Wide 8×32 scoreboard | ESP32-C3 | `board/ScoreBoard_Wide_8x32/` |
| **v1.1** | Scoreboard (serve dots) | ESP32-C6 | `board/ScoreBoard_V1_1-Board/` |
| **v1.1** | Remote (one per team) | ESP32-C6 | `remote/Scoreboard_V1_1-Remote/` |
| v2.0 (interim) | Scoreboard | ESP32-C6 | `board/ScoreBoard_V2_Final-Board/` |
| Tab5 | LCD scoreboard + C6 radio hook | M5Stack Tab5 / ESP32-C6 | `board/PadelScoreboardTab5/` |
| v1.0 | Scoreboard | ESP32-C3 | `board/ScoreBoard_V1_Final-Board/` |
| v1.0 | Remote (one for both teams) | ESP32-C6 | `remote/Scoreboard_Final_V1-Remote/` |

The sketches talk over ESP-NOW. No Wi-Fi router is required. v1.1 remotes are hardcoded to the scoreboard MAC printed on Serial at boot.

## Hardware (v1.1)

**Scoreboard (ESP32-C6)**
- 16×16 WS2812B matrix on **GPIO 6** (256 LEDs)
- Mode button on **GPIO 7**
- LED driver enable on **GPIO 23** (held HIGH; this board does not sleep)
- Battery ADC on **GPIO 0**

Yellow serve dots match the ESP32-C3 16×16 below. Pinout is unchanged.

**Remote (ESP32-C6, one unit per team)**
- WS2812 LED on **GPIO 7**
- Button 1 on **GPIO 6** (this team: score / reset / sleep, and **wake**)
- Button 2 on **GPIO 20** (undo / deathmatch / other-team mode)
- Battery ADC on **GPIO 0**

ESP32-C6 can only wake from deep sleep on GPIO 0–7. Button 1 (GPIO 6) is the wake button. Button 2 (GPIO 20) cannot wake the chip.

## 16×16 display (ESP32-C3)

Same v1.1 remote protocol and the same scoring rules as the ESP32-C6 board. This build has no LED-driver enable pin. A 3 s+ press blacks out. It does not sleep.

**Wiring**
- 16×16 WS2812B (256 LEDs) data on **GPIO 6**
- Mode button on **GPIO 5**, switch to GND
- Battery ADC on **GPIO 0**

Two yellow dots mark the server, on the outer column. Player 1 is the top two pixels. Player 2 is the pair between the game digit and the points. At 0-0 the first score press only chooses that side’s player 1. Service then changes each game. In a tie-break the due server serves one point, then each player serves two. The dots stay off in deathmatch and until a server has been chosen.

```bash
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,UploadSpeed=921600" \
  board/ScoreBoard_16x16_C3
```

## Wide display (ESP32-C3, 8×32)

Same v1.1 remote protocol. The board prints its own MAC on Serial. Remotes already aimed at that MAC stay paired. Do not paste this board's MAC into a remote that belongs to a different scoreboard.

**Wiring**
- 8×32 WS2812B (256 LEDs) data on **GPIO 6**
- Mode button on **GPIO 5**, switch to GND. This pin also wakes the board from deep sleep
- No LED-driver enable pin and no on-board battery ADC

The strip is column serpentine. LED 0 is the top of the leftmost column. The first 8 LEDs run down that full left column. Odd columns run bottom to top, so the wire continues along the bottom (bottom left, then bottom right) and ends toward the top right.

Blue is the left half, starting at the top left. Left to right the screen is: blue game, blue points, red points, red game.

Two orange dots mark the server, in the outer column of that side. Player 1 is the top two pixels. Player 2 is the two pixels just above that side’s battery. At 0-0 the first score press only chooses that side’s player 1. Service then changes each game. In a tie-break the due server serves one point, then each player serves two.

Boot scrolls PADEL PIXEL, then holds PADEL and PIXEL centred for 1.5 seconds each.

**Button**

| Press | Action |
|---|---|
| Short (&lt; 1 s) | Cycle score colours |
| 1–3 s | Toggle deathmatch |
| 3 s+ | Deep sleep (wake on GPIO 5) |

It also deep-sleeps after **40 minutes** with no scoring, undo, reset, deathmatch, or button press. Remote battery reports do not keep it awake. Wake reboots the board and clears the match. GPIO 5 is used because an ESP32-C3 can only deep-sleep-wake on GPIO 0–5.

```bash
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,UploadSpeed=921600" \
  board/ScoreBoard_Wide_8x32
```

## Scoreboard controls (v1.1)

| Board button | Action |
|---|---|
| Short (&lt; 1 s) | Cycle score colours |
| 1–3 s | Toggle deathmatch |
| 3 s+ | Blackout. One yellow/green pixel hops, and ESP-NOW stays up. Press again to restore the scores. |

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
| **v2.0-interim** | C6 scoreboard snapshot (`ScoreBoard_V2_Final-Board`) before v1.1 serve/blackout work |
| **v1.1** | C6 scoreboard: white-first colours, 20 ms team flash, undo/deathmatch from remote, MAC on Serial, yellow serve dots; per-team remotes with v1.1 buttons, 5 s breathe, GPIO wake |
| **wide-8x32** | C3 wide scoreboard: 8×32 column-serpentine matrix, same v1.1 remotes, 40-minute sleep, GPIO 5 wake |
| **v1.2** | 16×16 ESP32-C3 (GPIO 6 matrix, GPIO 5 button, yellow serve dots, blackout) and wide 8×32 (GPIO 6 matrix, GPIO 5 wake, orange serve dots, PADEL then PIXEL after the boot scroll). Remotes stay on the v1.1 protocol |
| **tab5** | M5Stack Tab5 LCD scoreboard + ESP32-C6 padel radio hooks; prebuilt bins under `firmware/` |

## Prebuilt firmware

Flashable binaries for the Tab5 build live in `firmware/`. They match the Tab5 sources from 2026-10-23.

| File | What |
|---|---|
| `firmware/PadelScoreboardTab5.ino.merged.bin` | Preferred full Tab5 flash image (flash at 0x0) |
| `firmware/PadelScoreboardTab5.ino.bin` | Tab5 app partition only |
| `firmware/PadelScoreboardTab5.ino.bootloader.bin` | Tab5 bootloader |
| `firmware/PadelScoreboardTab5.ino.partitions.bin` | Tab5 partition table |
| `firmware/PadelScoreboardTab5-c6-network_adapter.bin` | ESP32-C6 network-adapter / ESP-Hosted build with padel hooks |

Arduino ESP32-C3 / ESP32-C6 scoreboard and remote sketches have **no matching prebuilt `.bin` in this repo yet** (sources are current; rebuild locally with the `arduino-cli` commands below).

## Tab5 scoreboard

See `board/PadelScoreboardTab5/README.md`. The Arduino sketch under `arduino/PadelScoreboardTab5/` is the one to flash to the Tab5. `src/` is the same code for PlatformIO / host test. `c6/main/padel.*` are the padel-specific hooks for the ESP32-C6 coprocessor; the rest of the ESP-Hosted vendor tree is not vendored here.

## ScoreBoard V2 Final (interim C6)

`board/ScoreBoard_V2_Final-Board/` is the July 2026 ESP32-C6 board that sits between v1.0 and v1.1. Prefer `ScoreBoard_V1_1-Board` for new flashes (serve dots, blackout hop, same pinout).

