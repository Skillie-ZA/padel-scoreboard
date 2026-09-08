# Padel Scoreboard

ESP-NOW padel scoreboard and handheld remote.

This repository starts at **v1.0**. Later versions will be tagged on the same repo (`v1.1`, …).

## What’s in v1.0

| Part | Board | Sketch |
|---|---|---|
| Scoreboard | ESP32-C3 | `board/ScoreBoard_V1_Final-Board/` |
| Remote | ESP32-C6 | `remote/Scoreboard_Final_V1-Remote/` |

The two sketches talk over ESP-NOW (broadcast). No Wi-Fi router is required.

## Hardware

**Scoreboard**
- ESP32-C3
- 16×16 WS2812B matrix on **GPIO 6** (256 LEDs)
- Mode button on **GPIO 7** (toggles deathmatch)

**Remote**
- ESP32-C6
- WS2812 LED on **GPIO 7**
- Button 1 (team 1 / blue) on **GPIO 6**
- Button 2 (team 2 / red) on **GPIO 20**
- Battery ADC on **GPIO 0**

## Remote controls (v1.0)

| Press | Action |
|---|---|
| Short (&lt; 2 s) | Score a point for that button’s team |
| 2–4 s | Reset the match |
| 4 s+ | Sleep (LED turns yellow first) |

On boot the remote LED flashes blue, then red. A successful send flashes green. Heartbeat is blue then red every 5 seconds. After 20 minutes of no presses it sleeps.

## ESP-NOW message

```text
cmd:  0 = score, 99 = reset, 100 = battery, 101 = sleep
team: 1 or 2
data: battery percent (cmd 100) or unused
```

## Flash with Arduino IDE

1. Install [Arduino IDE](https://www.arduino.cc/en/software) and the **esp32** board package.
2. Install the **FastLED** library.
3. Open the sketch folder (folder name must match the `.ino` name).
4. Select the board and port, then Upload.

**Scoreboard:** ESP32-C3  
**Remote:** ESP32-C6, USB CDC on boot enabled if you use the native USB port.

Command-line example for the remote:

```bash
arduino-cli compile --upload -p /dev/cu.usbmodem101 \
  --fqbn "esp32:esp32:esp32c6:CDCOnBoot=cdc,UploadSpeed=921600" \
  remote/Scoreboard_Final_V1-Remote
```

## Versioning

| Tag | Notes |
|---|---|
| **v1.0** | First snapshot: C3 scoreboard + two-button C6 remote |
