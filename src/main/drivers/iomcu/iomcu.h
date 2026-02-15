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

#ifdef USE_IOMCU

#include <stdint.h>
#include <stdbool.h>

#include "drivers/time.h"

/**
 * Initialize IOMCU driver
 * - Opens UART6 communication
 * - Verifies IOMCU firmware CRC
 * - Uploads new firmware if needed
 *
 * @return true if IOMCU initialized successfully, false otherwise
 */
bool iomcuInit(void);

/**
 * IOMCU task - call at 50Hz from scheduler
 * - Sends PWM values to IOMCU
 * - Reads status from IOMCU
 * - Monitors health
 */
void iomcuTask(timeUs_t currentTimeUs);

/**
 * Check if IOMCU was successfully initialized
 * @return true if IOMCU initialization completed
 */
bool iomcuIsInitialized(void);

/**
 * Check if IOMCU is healthy
 * @return true if communication is working and no errors
 */
bool iomcuIsHealthy(void);

/**
 * Write PWM value to IOMCU channel
 * @param channel Channel number (0-7)
 * @param value PWM value (1000-2000)
 */
void iomcuWritePwm(uint8_t channel, uint16_t value);

/**
 * Write all PWM values to IOMCU at once (more efficient)
 * @param values Array of 8 PWM values
 */
void iomcuWritePwmAll(const uint16_t *values);

/**
 * Set IOMCU output mode
 * @param mode Output mode (PWM_MODE_*)
 */
void iomcuSetOutputMode(uint8_t mode);

/**
 * Set IOMCU PWM rate
 * @param rate Update rate in Hz (50-400)
 */
void iomcuSetPwmRate(uint16_t rate);

/**
 * Force safety off (override button)
 */
void iomcuForceSafetyOff(void);

/**
 * Check if safety is off
 * @return true if outputs are enabled
 */
bool iomcuIsSafetyOff(void);

/**
 * Force firmware update
 * @return true if update successful
 */
bool iomcuForceUpdate(void);

/**
 * Get number of IOMCU channels available
 * @return 8 if IOMCU is working, 0 otherwise
 */
uint8_t iomcuGetChannelCount(void);

#endif // USE_IOMCU
