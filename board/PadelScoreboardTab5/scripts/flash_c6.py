#!/usr/bin/env python3
"""Stream a C6 app image to the Tab5 over USB. The P4 forwards it with esp-hosted OTA.

Each 1024-byte chunk waits for "C6OTA WROTE" before the next one is sent. A write
that does not return stops the transfer, so a hung RPC cannot discard the image.
"""

import sys
import time

import serial


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: flash_c6.py PORT IMAGE", file=sys.stderr)
        return 2
    port, path = sys.argv[1], sys.argv[2]
    data = open(path, "rb").read()
    if len(data) < 1024:
        print("image is too small", file=sys.stderr)
        return 2
    # Opening the port resets the P4. Do not pulse RTS: that drops USB mid-transfer.
    ser = serial.Serial(port, 115200, timeout=0.05)
    ser.dtr = False
    ser.rts = False
    if not wait_for(ser, b"Waiting for the C6 score pipe", 25):
        print("tablet did not reach the score pipe", file=sys.stderr)
        return 1
    print("tablet is up", flush=True)
    ser.write(f"C6OTA {len(data)}\n".encode())
    ser.flush()
    if not wait_for(ser, b"C6OTA READY", 15, fail=b"C6OTA FAIL"):
        print("no READY from the tablet", file=sys.stderr)
        return 1
    print("streaming", len(data), "bytes", flush=True)
    sent = 0
    view = memoryview(data)
    for off in range(0, len(data), 1024):
        piece = view[off : off + 1024]
        # Stay under the USB RX queue even if the tablet is busy drawing.
        for bite in range(0, len(piece), 128):
            ser.write(piece[bite : bite + 128])
            ser.flush()
            time.sleep(0.01)
        sent += len(piece)
        marker = f"C6OTA WROTE {sent} ".encode()
        if not wait_for(ser, marker, 20, fail=b"C6OTA FAIL"):
            print(f"write of {sent} bytes did not finish", file=sys.stderr)
            return 1
        if sent % (64 * 1024) == 0 or sent == len(data):
            print(f"acked {sent}", flush=True)
    if not wait_for(ser, b"C6OTA OK", 180, fail=b"C6OTA FAIL"):
        print("timed out waiting for OK", file=sys.stderr)
        return 1
    print("C6 image activated", flush=True)
    return 0


def wait_for(ser: serial.Serial, token: bytes, seconds: float, fail: bytes | None = None) -> bool:
    buf = bytearray()
    printed = 0
    deadline = time.time() + seconds
    while time.time() < deadline:
        waiting = ser.in_waiting
        if waiting:
            buf += ser.read(waiting)
        else:
            time.sleep(0.02)
        printed = show_new_lines(buf, printed)
        if token in buf:
            return True
        if fail and fail in buf:
            return False
        if len(buf) > 8192:
            tail = bytes(buf[printed:])
            if len(tail) > 512:
                tail = tail[-512:]
            buf = bytearray(tail)
            printed = 0
    return token in buf


def show_new_lines(buf: bytearray, printed: int) -> int:
    while True:
        nl = buf.find(b"\n", printed)
        if nl < 0:
            return printed
        line = bytes(buf[printed:nl]).decode("utf-8", "replace")
        if "C6OTA" in line:
            print(line, flush=True)
        printed = nl + 1


if __name__ == "__main__":
    raise SystemExit(main())
