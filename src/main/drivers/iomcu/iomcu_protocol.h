/*
 * This file is part of INAV.
 *
 * INAV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * INAV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with INAV.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Protocol version
#define IOMCU_PROTOCOL_VERSION 1

// Board identification
#define IOMCU_BOARD_TYPE_IOV2 0x1001

// Maximum packet size
#define IOMCU_MAX_REGISTERS 22
#define IOMCU_PACKET_SIZE (4 + (IOMCU_MAX_REGISTERS * 2))

// Packet codes
#define IOMCU_CODE_READ  0
#define IOMCU_CODE_WRITE 1

// Register pages
#define PAGE_CONFIG       0
#define PAGE_STATUS       1
#define PAGE_DIRECT_PWM   54
#define PAGE_SETUP        50
#define PAGE_DSHOT        202

// Page 0: CONFIG registers (read-only)
#define REG_CONFIG_PROTOCOL_VERSION  0   // Protocol version
#define REG_CONFIG_BOARD_TYPE        1   // Board type ID
#define REG_CONFIG_MAX_CHANNELS      2   // Number of PWM channels
#define REG_CONFIG_FW_VERSION        3   // Firmware version

// Page 1: STATUS registers (read-only)
#define REG_STATUS_FLAGS             0   // Status flags (see STATUS_FLAG_*)
#define REG_STATUS_RX_ERRORS         1   // Protocol RX error count
#define REG_STATUS_SAFETY_STATE      2   // Safety button state

// STATUS flags
#define STATUS_FLAG_SAFETY_OFF       (1 << 0)  // Safety is disabled, outputs enabled
#define STATUS_FLAG_PWM_ENABLED      (1 << 1)  // PWM outputs are enabled
#define STATUS_FLAG_FMU_OK           (1 << 2)  // Communication with FMU is OK
#define STATUS_FLAG_FAILSAFE         (1 << 3)  // In failsafe mode

// Page 50: SETUP registers (read/write)
#define REG_SETUP_OUTPUT_MODE        0   // Output mode (PWM_MODE_*)
#define REG_SETUP_PWM_RATE           1   // PWM update rate in Hz
#define REG_SETUP_CRC_LOW            2   // Firmware CRC32 (bits 0-15)
#define REG_SETUP_CRC_HIGH           3   // Firmware CRC32 (bits 16-31)
#define REG_SETUP_REBOOT_BL          10  // Reboot to bootloader (write magic)
#define REG_SETUP_FORCE_SAFETY_OFF   11  // Force safety off (write magic)

// Output modes for REG_SETUP_OUTPUT_MODE
#define PWM_MODE_STANDARD       0    // Standard PWM (50-400Hz)
#define PWM_MODE_ONESHOT125     1    // OneShot125
#define PWM_MODE_DSHOT150       2    // DShot150
#define PWM_MODE_DSHOT300       3    // DShot300
#define PWM_MODE_DSHOT600       4    // DShot600

// Magic values
#define REBOOT_BL_MAGIC         14662   // Write to REG_SETUP_REBOOT_BL to reboot to bootloader
#define FORCE_SAFETY_OFF_MAGIC  22027   // Write to REG_SETUP_FORCE_SAFETY_OFF to disable safety

// Page 54: DIRECT_PWM registers (write-only)
// Array of 8 x 16-bit PWM values (1000-2000 range)
#define MAX_IOMCU_CHANNELS      8

// Page 202: DSHOT registers (write-only)
// Array of 8 x 16-bit DShot throttle values (0-2047 range)

// Bootloader protocol commands
#define PROTO_NOP               0x00
#define PROTO_OK                0x10
#define PROTO_FAILED            0x11
#define PROTO_INSYNC            0x12
#define PROTO_INVALID           0x13
#define PROTO_BAD_SILICON_REV   0x14
#define PROTO_EOC               0x20
#define PROTO_GET_SYNC          0x21
#define PROTO_GET_DEVICE        0x22
#define PROTO_CHIP_ERASE        0x23
#define PROTO_CHIP_VERIFY       0x24
#define PROTO_PROG_MULTI        0x27
#define PROTO_READ_MULTI        0x28
#define PROTO_GET_CRC           0x29
#define PROTO_REBOOT            0x30

// Bootloader info queries
#define INFO_BL_REV             1
#define INFO_BOARD_ID           2
#define INFO_FLASH_SIZE         4

// Maximum bootloader protocol values
#define BL_REV                  5
#define PROG_MULTI_MAX          248

// Communication parameters
#define IOMCU_BAUD_RATE         1500000   // 1.5 Mbps runtime
#define IOMCU_BOOTLOADER_BAUD   115200    // Bootloader baud rate
#define IOMCU_WATCHDOG_MS       1000      // FMU watchdog timeout
#define IOMCU_FLASH_SIZE        (124*1024) // 124KB (128KB - 4KB bootloader)

/**
 * IOMCU packet structure
 *
 * Byte 0: count_code
 *   bits 0-5: register count (0-63)
 *   bits 6-7: operation code (0=READ, 1=WRITE)
 * Byte 1: CRC-8 checksum (Dallas/Maxim polynomial 0x07)
 * Byte 2: page number (0-255)
 * Byte 3: register offset within page (0-255)
 * Bytes 4+: register data (count * 2 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t count_code;              // Count (6 bits) | Code (2 bits)
    uint8_t crc;                     // CRC-8 checksum
    uint8_t page;                    // Register page
    uint8_t offset;                  // Register offset
    uint16_t regs[IOMCU_MAX_REGISTERS]; // Register values
} iomcuPacket_t;

/**
 * CRC-8 calculation using Dallas/Maxim polynomial (0x07)
 *
 * @param crc Initial CRC value (0 for new calculation)
 * @param data Pointer to data bytes
 * @param len Number of bytes
 * @return Updated CRC value
 */
static inline uint8_t iomcuCrc8(uint8_t crc, const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * Calculate CRC for an IOMCU packet
 *
 * @param pkt Pointer to packet
 * @return Calculated CRC-8 value
 */
static inline uint8_t iomcuPacketCrc(const iomcuPacket_t *pkt)
{
    uint8_t count = pkt->count_code & 0x3F;
    uint8_t crc = iomcuCrc8(0, (const uint8_t*)pkt, 1);  // count_code
    crc = iomcuCrc8(crc, (const uint8_t*)&pkt->page, 1);  // page
    crc = iomcuCrc8(crc, (const uint8_t*)&pkt->offset, 1); // offset
    crc = iomcuCrc8(crc, (const uint8_t*)pkt->regs, count * 2); // data
    return crc;
}

/**
 * Verify packet CRC
 *
 * @param pkt Pointer to packet
 * @return true if CRC is valid
 */
static inline bool iomcuPacketCrcValid(const iomcuPacket_t *pkt)
{
    return pkt->crc == iomcuPacketCrc(pkt);
}

/**
 * Get packet operation code
 */
static inline uint8_t iomcuPacketCode(const iomcuPacket_t *pkt)
{
    return (pkt->count_code >> 6) & 0x03;
}

/**
 * Get packet register count
 */
static inline uint8_t iomcuPacketCount(const iomcuPacket_t *pkt)
{
    return pkt->count_code & 0x3F;
}

/**
 * Set packet operation code and count
 */
static inline void iomcuPacketSetCodeCount(iomcuPacket_t *pkt, uint8_t code, uint8_t count)
{
    pkt->count_code = ((code & 0x03) << 6) | (count & 0x3F);
}
