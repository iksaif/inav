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

#include "iomcu_registers.h"
#include <string.h>

// Register page storage
// Page 0: CONFIG (read-only)
static uint16_t regConfig[16] = {
    [REG_CONFIG_PROTOCOL_VERSION] = IOMCU_PROTOCOL_VERSION,
    [REG_CONFIG_BOARD_TYPE] = IOMCU_BOARD_TYPE_IOV2,
    [REG_CONFIG_MAX_CHANNELS] = MAX_IOMCU_CHANNELS,
    [REG_CONFIG_FW_VERSION] = IOMCU_FW_VERSION,
};

// Page 1: STATUS (read-only, updated dynamically)
static uint16_t regStatus[16] = {0};

// Page 50: SETUP (read/write)
static uint16_t regSetup[16] = {
    [REG_SETUP_OUTPUT_MODE] = PWM_MODE_STANDARD,
    [REG_SETUP_PWM_RATE] = 50,  // Default 50Hz
};

// Page 54: DIRECT_PWM (write-only)
static uint16_t regDirectPwm[MAX_IOMCU_CHANNELS] = {
    1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500  // Neutral values
};

// Page 202: DSHOT (write-only)
static uint16_t regDshot[MAX_IOMCU_CHANNELS] = {0};

// External references (provided by other modules)
extern bool safetyIsOff(void);
extern uint8_t safetyGetState(void);
extern bool fmuIsHealthy(void);
extern bool isFailsafe(void);

// Forward declarations for bootloader
extern void rebootToBootloader(void);

// Internal state
static bool rebootRequested = false;

void registersInit(void)
{
    // Reset dynamic state
    memset(regStatus, 0, sizeof(regStatus));

    // Initialize status
    registersUpdateStatus();
}

void registersUpdateStatus(void)
{
    // Build status flags
    uint16_t flags = 0;

    if (safetyIsOff()) {
        flags |= STATUS_FLAG_SAFETY_OFF;
        flags |= STATUS_FLAG_PWM_ENABLED;
    }

    if (fmuIsHealthy()) {
        flags |= STATUS_FLAG_FMU_OK;
    }

    if (isFailsafe()) {
        flags |= STATUS_FLAG_FAILSAFE;
    }

    regStatus[REG_STATUS_FLAGS] = flags;
    regStatus[REG_STATUS_SAFETY_STATE] = safetyGetState();
}

void registersSetFirmwareCrc(uint32_t crc32)
{
    regSetup[REG_SETUP_CRC_LOW] = (uint16_t)(crc32 & 0xFFFF);
    regSetup[REG_SETUP_CRC_HIGH] = (uint16_t)(crc32 >> 16);
}

bool registersRead(uint8_t page, uint8_t offset, uint8_t count, uint16_t *values)
{
    if (count == 0 || count > 22 || values == NULL) {
        return false;
    }

    switch (page) {
        case PAGE_CONFIG:
            if (offset + count > sizeof(regConfig) / sizeof(regConfig[0])) {
                return false;
            }
            memcpy(values, &regConfig[offset], count * sizeof(uint16_t));
            return true;

        case PAGE_STATUS:
            if (offset + count > sizeof(regStatus) / sizeof(regStatus[0])) {
                return false;
            }
            // Update status before reading
            registersUpdateStatus();
            memcpy(values, &regStatus[offset], count * sizeof(uint16_t));
            return true;

        case PAGE_SETUP:
            if (offset + count > sizeof(regSetup) / sizeof(regSetup[0])) {
                return false;
            }
            memcpy(values, &regSetup[offset], count * sizeof(uint16_t));
            return true;

        case PAGE_DIRECT_PWM:
            if (offset + count > MAX_IOMCU_CHANNELS) {
                return false;
            }
            memcpy(values, &regDirectPwm[offset], count * sizeof(uint16_t));
            return true;

        case PAGE_DSHOT:
            if (offset + count > MAX_IOMCU_CHANNELS) {
                return false;
            }
            memcpy(values, &regDshot[offset], count * sizeof(uint16_t));
            return true;

        default:
            return false;
    }
}

bool registersWrite(uint8_t page, uint8_t offset, uint8_t count, const uint16_t *values)
{
    if (count == 0 || count > 22 || values == NULL) {
        return false;
    }

    switch (page) {
        case PAGE_CONFIG:
        case PAGE_STATUS:
            // Read-only pages
            return false;

        case PAGE_SETUP:
            if (offset + count > sizeof(regSetup) / sizeof(regSetup[0])) {
                return false;
            }

            // Handle special registers
            for (uint8_t i = 0; i < count; i++) {
                uint8_t reg = offset + i;
                uint16_t value = values[i];

                if (reg == REG_SETUP_REBOOT_BL) {
                    if (value == REBOOT_BL_MAGIC) {
                        rebootRequested = true;
                    }
                } else if (reg == REG_SETUP_FORCE_SAFETY_OFF) {
                    if (value == FORCE_SAFETY_OFF_MAGIC) {
                        extern void safetyForceOff(void);
                        safetyForceOff();
                    }
                } else {
                    regSetup[reg] = value;
                }
            }
            return true;

        case PAGE_DIRECT_PWM:
            if (offset + count > MAX_IOMCU_CHANNELS) {
                return false;
            }
            // Validate PWM range (1000-2000)
            for (uint8_t i = 0; i < count; i++) {
                uint16_t pwm = values[i];
                if (pwm < 1000) pwm = 1000;
                if (pwm > 2000) pwm = 2000;
                regDirectPwm[offset + i] = pwm;
            }
            return true;

        case PAGE_DSHOT:
            if (offset + count > MAX_IOMCU_CHANNELS) {
                return false;
            }
            // Validate DShot range (0-2047)
            for (uint8_t i = 0; i < count; i++) {
                uint16_t throttle = values[i];
                if (throttle > 2047) throttle = 2047;
                regDshot[offset + i] = throttle;
            }
            return true;

        default:
            return false;
    }
}

uint16_t registersGetPwmValue(uint8_t channel)
{
    if (channel >= MAX_IOMCU_CHANNELS) {
        return 1500;  // Neutral
    }
    return regDirectPwm[channel];
}

uint16_t registersGetDshotValue(uint8_t channel)
{
    if (channel >= MAX_IOMCU_CHANNELS) {
        return 0;
    }
    return regDshot[channel];
}

uint8_t registersGetOutputMode(void)
{
    uint16_t mode = regSetup[REG_SETUP_OUTPUT_MODE];
    if (mode > PWM_MODE_DSHOT600) {
        mode = PWM_MODE_STANDARD;
    }
    return (uint8_t)mode;
}

uint16_t registersGetPwmRate(void)
{
    uint16_t rate = regSetup[REG_SETUP_PWM_RATE];
    if (rate < 50) rate = 50;
    if (rate > 400) rate = 400;
    return rate;
}

void registersIncrementRxErrors(void)
{
    regStatus[REG_STATUS_RX_ERRORS]++;
}

bool registersCheckRebootRequest(void)
{
    return rebootRequested;
}

void registersClearRebootRequest(void)
{
    rebootRequested = false;
    regSetup[REG_SETUP_REBOOT_BL] = 0;
}
