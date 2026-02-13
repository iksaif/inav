#!/usr/bin/env python3
"""Detect what firmware is running on connected serial ports."""
import serial
import sys
import glob
import time

MAVLINK_MAGIC_V1 = 0xFE
MAVLINK_MAGIC_V2 = 0xFD

def is_mavlink(raw_bytes):
    """Check if raw bytes look like MAVLink traffic."""
    if len(raw_bytes) < 4:
        return False
    mavlink_count = sum(1 for b in raw_bytes if b in (MAVLINK_MAGIC_V1, MAVLINK_MAGIC_V2))
    printable = sum(1 for b in raw_bytes if 0x20 <= b <= 0x7E or b in (0x0A, 0x0D))
    total = len(raw_bytes)
    return mavlink_count >= 2 or (total > 10 and printable / total < 0.5)

def mavlink_version(raw_bytes):
    v1 = sum(1 for b in raw_bytes if b == MAVLINK_MAGIC_V1)
    v2 = sum(1 for b in raw_bytes if b == MAVLINK_MAGIC_V2)
    return "v2" if v2 > v1 else "v1"

def try_port(port, baudrate=115200):
    print(f"Trying {port} at {baudrate} baud...")
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(0.5)
        ser.reset_input_buffer()

        all_raw = bytearray()

        # Passive listen
        time.sleep(1)
        n = ser.in_waiting
        if n > 0:
            all_raw.extend(ser.read(n))

        # Poke it
        ser.write(b'\r\n')
        time.sleep(0.5)
        n = ser.in_waiting
        if n > 0:
            all_raw.extend(ser.read(n))

        if is_mavlink(all_raw):
            ver = mavlink_version(all_raw)
            headers = sum(1 for b in all_raw if b in (MAVLINK_MAGIC_V1, MAVLINK_MAGIC_V2))
            print(f"  -> MAVLink {ver} detected ({len(all_raw)} bytes, {headers} packet headers)")
            print(f"  -> This is ArduPilot or PX4 firmware")
            ser.close()
            return "mavlink"

        # Try INAV CLI
        ser.write(b'####\r\n')
        time.sleep(1)
        n = ser.in_waiting
        if n > 0:
            raw = ser.read(n)
            all_raw.extend(raw)
            if is_mavlink(raw):
                ver = mavlink_version(all_raw)
                headers = sum(1 for b in all_raw if b in (MAVLINK_MAGIC_V1, MAVLINK_MAGIC_V2))
                print(f"  -> MAVLink {ver} detected ({len(all_raw)} bytes, {headers} packet headers)")
                print(f"  -> This is ArduPilot or PX4 firmware")
                ser.close()
                return "mavlink"

        # Try version command
        ser.write(b'version\r\n')
        time.sleep(1)
        n = ser.in_waiting
        if n > 0:
            raw = ser.read(n)
            all_raw.extend(raw)
            if is_mavlink(raw):
                ver = mavlink_version(all_raw)
                headers = sum(1 for b in all_raw if b in (MAVLINK_MAGIC_V1, MAVLINK_MAGIC_V2))
                print(f"  -> MAVLink {ver} detected ({len(all_raw)} bytes, {headers} packet headers)")
                print(f"  -> This is ArduPilot or PX4 firmware")
                ser.close()
                return "mavlink"
            text = raw.decode('ascii', errors='replace')
            if text.strip():
                print(f"  Version: {text.strip()[:200]}")
                if 'inav' in text.lower():
                    ser.close()
                    return "inav"
                if 'betaflight' in text.lower():
                    ser.close()
                    return "betaflight"

        ser.close()

        if len(all_raw) == 0:
            print(f"  -> No data received (inactive port or bootloader)")
            return "no_data"
        print(f"  -> Unknown protocol ({len(all_raw)} bytes)")
        return "unknown"

    except serial.SerialException as e:
        print(f"  Error: {e}")
        return "error"

# Find all USB serial ports
ports = sorted(glob.glob('/dev/cu.usbmodem*'))
if not ports:
    print("No USB serial devices found!")
    print("Is the Pixhawk connected via USB?")
    sys.exit(1)

print(f"Found ports: {ports}\n")

results = {}
for port in ports:
    results[port] = try_port(port)
    print()

print("=" * 50)
print("Summary:")
for port, result in results.items():
    if result == "mavlink":
        print(f"  {port}: ArduPilot/PX4 (MAVLink)")
    elif result == "inav":
        print(f"  {port}: INAV")
    elif result == "betaflight":
        print(f"  {port}: Betaflight")
    elif result == "no_data":
        print(f"  {port}: No data (bootloader/debug/inactive)")
    elif result == "error":
        print(f"  {port}: Could not open")
    else:
        print(f"  {port}: {result}")
