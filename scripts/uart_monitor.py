#!/usr/bin/env python3
"""
uart_monitor.py
───────────────
PC-side serial monitor for the Heat Stress Monitoring System.

Usage:
    python3 uart_monitor.py --port /dev/ttyUSB0
    python3 uart_monitor.py --port COM3 --baud 9600 --log output.csv

Features:
  • Prints colour-coded readings to the terminal
  • Optionally saves timestamped readings to a CSV log file
  • Works on Windows (COM3), Linux (/dev/ttyUSB0), macOS (/dev/tty.usbserial-*)

Requirements:
    pip install pyserial colorama
"""

import argparse
import csv
import os
import re
import sys
from datetime import datetime

try:
    import serial
except ImportError:
    print("[ERROR] pyserial not found. Run: pip install pyserial")
    sys.exit(1)

try:
    from colorama import Fore, Style, init as colorama_init
    colorama_init(autoreset=True)
    HAS_COLOR = True
except ImportError:
    HAS_COLOR = False
    class Fore:
        GREEN = YELLOW = RED = WHITE = CYAN = ""
    class Style:
        BRIGHT = RESET_ALL = ""

# ── ANSI colour helpers ──────────────────────────────────────────────────────
STATUS_COLOR = {
    "SAFE":    Fore.GREEN  + Style.BRIGHT,
    "WARNING": Fore.YELLOW + Style.BRIGHT,
    "DANGER":  Fore.RED    + Style.BRIGHT,
}

def colorize(text, status):
    color = STATUS_COLOR.get(status, "")
    return f"{color}{text}{Style.RESET_ALL}" if HAS_COLOR else text

# ── Parser ───────────────────────────────────────────────────────────────────
def parse_packet(lines: list[str]) -> dict | None:
    """
    Parse a 4-line UART packet into a dict.

    Expected format:
        Temp: 13.18 C
        Humi: 41.01 %
        Gas:  259.76 ppm
        Status: SAFE
    """
    data = {}
    patterns = {
        "temp":     r"Temp:\s+([\d.]+)\s*C",
        "humidity": r"Humi:\s+([\d.]+)\s*%",
        "gas":      r"Gas:\s+([\d.]+)\s*ppm",
        "status":   r"Status:\s+(\w+)",
    }
    blob = " ".join(lines)
    for key, pat in patterns.items():
        m = re.search(pat, blob)
        if m:
            data[key] = m.group(1)
    return data if len(data) == 4 else None


def print_reading(data: dict, index: int):
    ts = datetime.now().strftime("%H:%M:%S")
    status = data["status"]
    col = STATUS_COLOR.get(status, "")
    reset = Style.RESET_ALL if HAS_COLOR else ""

    print(f"\n{Fore.CYAN}[{ts}] Reading #{index}{reset}")
    print(f"  Temp     : {data['temp']} °C")
    print(f"  Humidity : {data['humidity']} %")
    print(f"  Gas      : {data['gas']} ppm")
    print(f"  Status   : {col}{status}{reset}")


def write_csv_header(writer):
    writer.writerow(["timestamp", "temp_C", "humidity_pct", "gas_ppm", "status"])


def main():
    ap = argparse.ArgumentParser(description="Heat Stress UART Monitor")
    ap.add_argument("--port",  default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0)")
    ap.add_argument("--baud",  type=int, default=9600,  help="Baud rate (default: 9600)")
    ap.add_argument("--log",   default=None,            help="Optional CSV log file path")
    args = ap.parse_args()

    print(f"{Style.BRIGHT}Heat Stress Monitor — UART Logger{Style.RESET_ALL}")
    print(f"Port: {args.port}  |  Baud: {args.baud}")
    if args.log:
        print(f"Logging to: {args.log}")
    print("Press Ctrl+C to stop.\n")

    csv_file   = None
    csv_writer = None
    if args.log:
        csv_file   = open(args.log, "w", newline="")
        csv_writer = csv.writer(csv_file)
        write_csv_header(csv_writer)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
    except serial.SerialException as e:
        print(f"[ERROR] Cannot open {args.port}: {e}")
        sys.exit(1)

    buffer = []
    reading_index = 0

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("ascii", errors="ignore").strip()

            if line.startswith("-----"):
                buffer.clear()
                continue

            if line:
                buffer.append(line)

            if len(buffer) >= 4:
                data = parse_packet(buffer)
                if data:
                    reading_index += 1
                    print_reading(data, reading_index)
                    if csv_writer:
                        ts = datetime.now().isoformat(timespec="seconds")
                        csv_writer.writerow([
                            ts, data["temp"], data["humidity"],
                            data["gas"], data["status"]
                        ])
                        csv_file.flush()
                buffer.clear()

    except KeyboardInterrupt:
        print("\n\nMonitor stopped.")
    finally:
        ser.close()
        if csv_file:
            csv_file.close()
        if reading_index:
            print(f"Total readings captured: {reading_index}")


if __name__ == "__main__":
    main()
