#!/usr/bin/env python3
"""
Convert binary file to C array for embedding in firmware.
Used to embed IOMCU firmware binary into FMU firmware.
"""

import sys
import os

def bin_to_c(input_file, output_file, array_name):
    """
    Convert binary file to C source file with byte array.

    Args:
        input_file: Path to input binary file
        output_file: Path to output C source file
        array_name: Name of the C array variable
    """
    # Read binary file
    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except IOError as e:
        print(f"Error reading input file: {e}", file=sys.stderr)
        return False

    # Generate C source
    try:
        with open(output_file, 'w') as f:
            # File header
            f.write("/*\n")
            f.write(" * Auto-generated file - DO NOT EDIT\n")
            f.write(f" * Generated from: {os.path.basename(input_file)}\n")
            f.write(f" * Size: {len(data)} bytes\n")
            f.write(" */\n\n")

            f.write("#include <stdint.h>\n\n")

            # Array definition
            f.write(f"const uint8_t {array_name}[] = {{\n")

            # Write data in rows of 12 bytes
            for i in range(0, len(data), 12):
                chunk = data[i:i+12]
                f.write("    ")
                f.write(", ".join(f"0x{byte:02x}" for byte in chunk))
                if i + 12 < len(data):
                    f.write(",")
                f.write("\n")

            f.write("};\n\n")

            # Size variable
            f.write(f"const uint32_t {array_name}_size = sizeof({array_name});\n")

    except IOError as e:
        print(f"Error writing output file: {e}", file=sys.stderr)
        return False

    print(f"Generated {output_file}: {len(data)} bytes")
    return True

def main():
    if len(sys.argv) != 4:
        print("Usage: bin2c.py <input.bin> <output.c> <array_name>")
        print("Example: bin2c.py iomcu_firmware.bin iomcu_firmware_embedded.c iomcu_firmware_bin")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    array_name = sys.argv[3]

    if not os.path.exists(input_file):
        print(f"Error: Input file not found: {input_file}", file=sys.stderr)
        sys.exit(1)

    if not bin_to_c(input_file, output_file, array_name):
        sys.exit(1)

if __name__ == "__main__":
    main()
