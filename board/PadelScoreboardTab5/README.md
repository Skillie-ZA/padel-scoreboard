# Padel Scoreboard for the M5Stack Tab5

The v1.1 padel scoreboard, drawn on the Tab5's 1280×720 display. The match rules and the ESP-NOW remotes are the same as the 16×16 board. There is no LED matrix.

## Remotes

The existing v1.1 remotes work once they are aimed at this tablet. On boot the screen shows the tablet's Wi-Fi MAC, and Serial prints a line ready to paste:

```text
uint8_t scoreboardAddress[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
```

Put that array in `Scoreboard_V1_1-Remote.ino` and flash each remote. Use the tablet's own MAC. Blue is `TEAM 1`, red is `TEAM 2`. The tablet stays on channel 1 and does not join a Wi-Fi network.

The Tab5's ESP32-P4 has no radio. The match, the serve, undo, deathmatch, and the ESP-NOW remotes run on the onboard ESP32-C6. The P4 only draws the snapshot the C6 sends over esp-hosted, and it sends taps back as the same 3-byte command. Until that pipe is up the header says **NO LINK** and taps score on the tablet itself. After the first snapshot the C6 is the only copy of the match.

The C6 image is `c6/`. It is the esp-hosted 2.12.3 SDIO slave plus the match. The C6 SDIO pins are fixed by the chip (CLK 19, CMD 18, D0 20, D1 21, D2 22, D3 23) and reset is the C6 EN pin, which the P4 already pulses. Build it with ESP-IDF 5.5:

```bash
./scripts/build_c6.sh
```

Flash it from the tablet's USB port. The P4 streams the app image to the C6. This replaces the factory Wi-Fi slave. M5 documents a restore: the Tab5 PCB has a C6 download header, and M5Burner can put the factory Wi-Fi firmware back. See https://docs.m5stack.com/en/guide/tab5/c6_wifi_restore

```bash
python3 scripts/flash_c6.py /dev/cu.usbmodem101 build/c6/network_adapter.bin
```

The remote sketch is unchanged apart from the MAC above. Remotes still have to be flashed with that MAC or they will keep talking to the other board.

| cmd | Meaning |
|---|---|
| 0 | Score that team |
| 1 | Undo |
| 99 | Reset the match |
| 100 | Remote battery percent |
| 101 | Remote is going to sleep |
| 102 | Toggle deathmatch |

## On the tablet

Blue is the left half, red is the right half. The current game is the large figure. The rally (0, 15, 30, 40, Ad, or golden point) sits underneath. Set scores are in the bar. Two yellow dots mark the server: player 1 near the top, player 2 in the middle. At 0-0 the first score only chooses that side's player 1.

Tap a half to score that side. The footer is undo, reset, deathmatch, the colour cycle, and blackout. Blackout leaves the radio on and hops one dot. Tap the screen to wake it.

A won game holds the set scores for 5 seconds. An odd game then says **CHANGE ENDS**.

## Rules worth knowing

Scoring, serve order, undo, deathmatch, and the colour cycle follow the 16×16 v1.1 firmware.

One correction: on the matrix board a won tie-break was checked against the game count. At 6-6 that count becomes 7-6, so the set never closed. Here, winning the tie-break wins the set, recorded as 7-6.

## Build

```bash
./tools/test_host.sh
./scripts/build.sh
```

The host test checks the match rules on this computer. The firmware build uses:

`m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc`

Boards marked v3.00 or newer can use `ChipVariant=postv3`.

Flash (hold Reset until the green LED blinks, then release):

```bash
arduino-cli upload -p /dev/cu.usbmodemXXXX \
  --fqbn "m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc" \
  arduino/PadelScoreboardTab5
arduino-cli monitor -p /dev/cu.usbmodemXXXX -c baudrate=115200
```

`src/` is the source of truth. `./scripts/sync_arduino.sh` copies it into the sketch folder.
