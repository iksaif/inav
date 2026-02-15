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

// Include shared protocol definitions
// When compiling IOMCU firmware, we'll use a local copy
#ifndef IOMCU_PROTOCOL_VERSION
    // Protocol constants (duplicated for IOMCU firmware build)
    #define IOMCU_PROTOCOL_VERSION 1
    #define IOMCU_BOARD_TYPE_IOV2 0x1001
    #define MAX_IOMCU_CHANNELS 8

    #define PAGE_CONFIG       0
    #define PAGE_STATUS       1
    #define PAGE_DIRECT_PWM   54
    #define PAGE_SETUP        50
    #define PAGE_DSHOT        202

    #define REG_CONFIG_PROTOCOL_VERSION  0
    #define REG_CONFIG_BOARD_TYPE        1
    #define REG_CONFIG_MAX_CHANNELS      2
    #define REG_CONFIG_FW_VERSION        3

    #define REG_STATUS_FLAGS             0
    #define REG_STATUS_RX_ERRORS         1
    #define REG_STATUS_SAFETY_STATE      2

    #define STATUS_FLAG_SAFETY_OFF       (1 << 0)
    #define STATUS_FLAG_PWM_ENABLED      (1 << 1)
    #define STATUS_FLAG_FMU_OK           (1 << 2)
    #define STATUS_FLAG_FAILSAFE         (1 << 3)

    #define REG_SETUP_OUTPUT_MODE        0
    #define REG_SETUP_PWM_RATE           1
    #define REG_SETUP_CRC_LOW            2
    #define REG_SETUP_CRC_HIGH           3
    #define REG_SETUP_REBOOT_BL          10
    #define REG_SETUP_FORCE_SAFETY_OFF   11

    #define PWM_MODE_STANDARD       0
    #define PWM_MODE_ONESHOT125     1
    #define PWM_MODE_DSHOT150       2
    #define PWM_MODE_DSHOT300       3
    #define PWM_MODE_DSHOT600       4

    #define REBOOT_BL_MAGIC         14662
    #define FORCE_SAFETY_OFF_MAGIC  22027
#endif

// Firmware version
#define IOMCU_FW_VERSION        1

/**
 * Initialize register system
 */
void registersInit(void);

/**
 * Read register(s) from a page
 *
 * @param page Page number
 * @param offset Register offset within page
 * @param count Number of registers to read
 * @param values Buffer to store register values
 * @return true if successful, false if invalid page/offset/count
 */
bool registersRead(uint8_t page, uint8_t offset, uint8_t count, uint16_t *values);

/**
 * Write register(s) to a page
 *
 * @param page Page number
 * @param offset Register offset within page
 * @param count Number of registers to write
 * @param values Buffer containing register values
 * @return true if successful, false if invalid page/offset/count or read-only
 */
bool registersWrite(uint8_t page, uint8_t offset, uint8_t count, const uint16_t *values);

/**
 * Update status flags
 * Called periodically to refresh dynamic status
 */
void registersUpdateStatus(void);

/**
 * Set firmware CRC in setup registers
 * @param crc32 CRC32 value of firmware
 */
void registersSetFirmwareCrc(uint32_t crc32);

/**
 * Get PWM output values from DIRECT_PWM page
 * @param channel Channel number (0-7)
 * @return PWM value (1000-2000)
 */
uint16_t registersGetPwmValue(uint8_t channel);

/**
 * Get DShot output values from DSHOT page
 * @param channel Channel number (0-7)
 * @return DShot throttle value (0-2047)
 */
uint16_t registersGetDshotValue(uint8_t channel);

/**
 * Get output mode from SETUP page
 * @return Output mode (PWM_MODE_*)
 */
uint8_t registersGetOutputMode(void);

/**
 * Get PWM rate from SETUP page
 * @return PWM update rate in Hz
 */
uint16_t registersGetPwmRate(void);

/**
 * Increment RX error counter
 */
void registersIncrementRxErrors(void);

/**
 * Check if reboot to bootloader was requested
 * @return true if reboot magic was written
 */
bool registersCheckRebootRequest(void);

/**
 * Clear reboot request flag
 */
void registersClearRebootRequest(void);
