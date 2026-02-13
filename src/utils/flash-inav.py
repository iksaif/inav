#!/usr/bin/env python3
"""
Flash INAV firmware to Pixhawk 6C via ArduPilot bootloader.

1. Detects ArduPilot on serial ports (MAVLink)
2. Sends proper MAVLink reboot-to-bootloader command via pymavlink
3. Monitors for bootloader (sync byte detection)
4. Flashes firmware via uploader.py
"""
import argparse
import serial
import time
import glob
import subprocess
import sys
import os

MAVLINK_MAGIC_V1 = 0xFE
MAVLINK_MAGIC_V2 = 0xFD
BOOTLOADER_SYNC = bytes([0x21, 0x20])
BOOTLOADER_INSYNC = 0x12
BOOTLOADER_OK = 0x10


def find_ports():
    return sorted(glob.glob('/dev/cu.usbmodem*'))


def is_mavlink(raw_bytes):
    if len(raw_bytes) < 4:
        return False
    mavlink_count = sum(1 for b in raw_bytes if b in (MAVLINK_MAGIC_V1, MAVLINK_MAGIC_V2))
    printable = sum(1 for b in raw_bytes if 0x20 <= b <= 0x7E or b in (0x0A, 0x0D))
    total = len(raw_bytes)
    return mavlink_count >= 2 or (total > 10 and printable / total < 0.5)


def detect_port_protocol(port, baud):
    """Quick protocol detection."""
    try:
        ser = serial.Serial(port, baud, timeout=1)
        time.sleep(0.3)
        ser.reset_input_buffer()
        time.sleep(0.5)
        raw = bytearray()
        n = ser.in_waiting
        if n > 0:
            raw.extend(ser.read(n))
        ser.write(b'\r\n')
        time.sleep(0.3)
        n = ser.in_waiting
        if n > 0:
            raw.extend(ser.read(n))
        ser.close()

        if is_mavlink(raw):
            return "mavlink"
        if len(raw) == 0:
            return "no_data"
        return "other"
    except:
        return "error"


def detect_bootloader(port):
    """Check if port has ArduPilot bootloader responding."""
    try:
        ser = serial.Serial(port, 115200, timeout=0.5)
        ser.reset_input_buffer()
        ser.write(BOOTLOADER_SYNC)
        time.sleep(0.3)
        resp = ser.read(2)
        ser.close()
        return len(resp) >= 2 and resp[0] == BOOTLOADER_INSYNC and resp[1] == BOOTLOADER_OK
    except:
        return False


def reboot_to_bootloader_mavlink(port, baud):
    """Use pymavlink to send proper reboot-to-bootloader command."""
    from pymavlink import mavutil

    print(f"    Connecting to {port} at {baud} baud...")
    try:
        mav = mavutil.mavlink_connection(port, baud=baud)
        print(f"    Waiting for heartbeat...")
        mav.wait_heartbeat(timeout=5)
        print(f"    Got heartbeat from system {mav.target_system}, component {mav.target_component}")

        print(f"    Sending reboot-to-bootloader command...")
        mav.mav.command_long_send(
            mav.target_system,
            mav.target_component,
            mavutil.mavlink.MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN,
            0,   # confirmation
            3,   # param1: 3 = stay in bootloader
            0, 0, 0, 0, 0, 0)

        time.sleep(0.5)
        mav.close()
        return True
    except Exception as e:
        print(f"    Error: {e}")
        try:
            mav.close()
        except:
            pass
        return False


def scan_for_bootloader(timeout=20):
    """Continuously scan all ports for bootloader."""
    start = time.time()
    seen_ports = set()
    while time.time() - start < timeout:
        ports = find_ports()
        for port in ports:
            if port not in seen_ports:
                seen_ports.add(port)
                print(f"\n    New port: {port}", end="", flush=True)
            if detect_bootloader(port):
                return port
        print(".", end="", flush=True)
        time.sleep(0.3)
    print()
    return None


def cmd_scan(args):
    """Scan ports and detect protocols."""
    print("Scanning serial ports...")
    ports = find_ports()
    if not ports:
        print("No USB serial devices found!")
        return

    print(f"Found {len(ports)} port(s)\n")
    for port in ports:
        proto = detect_port_protocol(port, args.baud)
        bl = detect_bootloader(port)
        label = {"mavlink": "ArduPilot/PX4 (MAVLink)",
                 "no_data": "No data",
                 "other": "Other/text",
                 "error": "Could not open"}.get(proto, proto)
        extras = []
        if bl:
            extras.append("BOOTLOADER ACTIVE")
        print(f"  {port}: {label}" + (f" [{', '.join(extras)}]" if extras else ""))


def cmd_flash(args):
    """Flash firmware."""
    firmware = args.firmware

    if not os.path.exists(firmware):
        print(f"Error: {firmware} not found!")
        sys.exit(1)

    if not firmware.endswith('.apj'):
        print(f"Error: firmware must be .apj format")
        print(f"Convert with: python3 bin2apj.py <input.bin> <output.apj>")
        sys.exit(1)

    print("=" * 55)
    print("  INAV Firmware Flasher - Pixhawk 6C")
    print("=" * 55)
    print()

    # Step 1: Scan ports
    print("Step 1: Scanning serial ports...")
    ports = args.port and [args.port] or find_ports()
    if not ports:
        print("  No USB serial devices found!")
        sys.exit(1)

    mavlink_ports = []
    for port in ports:
        proto = detect_port_protocol(port, args.baud)
        label = {"mavlink": "ArduPilot/PX4 (MAVLink)",
                 "no_data": "No data",
                 "other": "Other/text",
                 "error": "Could not open"}.get(proto, proto)
        print(f"  {port}: {label}")
        if proto == "mavlink":
            mavlink_ports.append(port)
        if detect_bootloader(port):
            print(f"  -> Bootloader already active!")
            flash(port, firmware, args)
            return
    print()

    # Step 2: Reboot to bootloader
    if args.no_reboot:
        print("Step 2: Skipping reboot (--no-reboot)")
    elif not mavlink_ports:
        print("Step 2: No MAVLink ports found.")
        print("  Will monitor for bootloader - please unplug and replug USB.")
    else:
        print(f"Step 2: Rebooting to bootloader via pymavlink...")
        rebooted = False
        for port in mavlink_ports:
            if reboot_to_bootloader_mavlink(port, args.baud):
                rebooted = True
                break

        if not rebooted:
            print("  Failed to send reboot command.")
            print("  Will monitor for bootloader - please unplug and replug USB.")

    # Step 3: Wait for bootloader
    print()
    timeout = args.timeout
    print(f"Step 3: Scanning for bootloader ({timeout}s timeout)", end="", flush=True)
    time.sleep(1)

    bl_port = scan_for_bootloader(timeout=timeout)

    if not bl_port:
        print()
        print("  Bootloader not found after reboot.")
        print()
        print("  Let's try manual power cycle:")
        print("  -> UNPLUG USB from Pixhawk now")
        input("  -> Press ENTER when unplugged...")
        print("  -> Now PLUG IN USB")
        print(f"  -> Scanning ({timeout}s timeout)", end="", flush=True)
        bl_port = scan_for_bootloader(timeout=timeout)

    if not bl_port:
        print()
        print("  ERROR: Could not find bootloader!")
        print()
        print("  Try:")
        print("  - flash-inav.py flash --no-reboot <firmware>")
        print("    (then unplug/replug during scanning)")
        print("  - INAV Configurator Firmware Flasher")
        print("  - ST-Link debugger via debug port")
        sys.exit(1)

    flash(bl_port, firmware, args)


def flash(bl_port, firmware, args):
    """Flash firmware via ArduPilot bootloader."""
    print()
    print(f"Step 4: Flashing {os.path.basename(firmware)} via {bl_port}...")
    print()

    cmd = [
        sys.executable, os.path.join(os.path.dirname(__file__), "uploader.py"),
        "--port", bl_port,
        "--baud-bootloader-flash", str(args.flash_baud),
        firmware
    ]
    result = subprocess.run(cmd)

    if result.returncode == 0:
        print()
        print("=" * 55)
        print("  SUCCESS! INAV firmware flashed!")
        print("  Wait 10 seconds for INAV to boot.")
        print("  Then connect via INAV Configurator.")
        print("=" * 55)
    else:
        print()
        print("  Flash failed! Check output above.")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Flash INAV firmware to Pixhawk via ArduPilot bootloader",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
examples:
  %(prog)s scan                          # Detect connected boards
  %(prog)s flash firmware.apj            # Auto-detect, reboot, and flash
  %(prog)s flash firmware.apj -p /dev/cu.usbmodem11201
                                         # Flash using specific port
  %(prog)s flash firmware.apj --no-reboot
                                         # Skip MAVLink reboot, manual power cycle
  %(prog)s flash firmware.apj --timeout 30
                                         # Wait longer for bootloader

converting firmware:
  python3 bin2apj.py inav.bin inav.apj   # Convert .bin to .apj format
  python3 bin2apj.py inav.bin inav.apj 56
                                         # With explicit board ID (56 = FMUv6C)
""")

    parser.add_argument("-b", "--baud", type=int, default=115200,
                        help="Serial baud rate for MAVLink (default: 115200)")

    subparsers = parser.add_subparsers(dest="command", help="Command to run")

    # scan subcommand
    subparsers.add_parser("scan", help="Scan serial ports and detect firmware")

    # flash subcommand
    flash_parser = subparsers.add_parser("flash",
                                         help="Flash firmware via ArduPilot bootloader")
    flash_parser.add_argument("firmware",
                              help="Path to .apj firmware file")
    flash_parser.add_argument("-p", "--port",
                              help="Serial port (auto-detect if not specified)")
    flash_parser.add_argument("--no-reboot", action="store_true",
                              help="Skip MAVLink reboot, wait for manual power cycle")
    flash_parser.add_argument("--timeout", type=int, default=20,
                              help="Bootloader detection timeout in seconds (default: 20)")
    flash_parser.add_argument("--flash-baud", type=int, default=115200,
                              help="Baud rate for bootloader flash (default: 115200)")

    args = parser.parse_args()

    if args.command == "scan":
        cmd_scan(args)
    elif args.command == "flash":
        cmd_flash(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
