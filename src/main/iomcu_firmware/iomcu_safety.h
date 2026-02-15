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

/**
 * Safety button states
 */
typedef enum {
    SAFETY_STATE_SAFE = 0,      // Outputs disabled
    SAFETY_STATE_ARMED = 1,     // Outputs enabled
} safetyState_t;

/**
 * Initialize safety button GPIO
 */
void safetyInit(void);

/**
 * Update safety button state
 * Call this periodically (e.g., every 10ms)
 */
void safetyUpdate(void);

/**
 * Check if safety is off (outputs enabled)
 * @return true if safety is off, false if safe
 */
bool safetyIsOff(void);

/**
 * Get current safety state
 * @return Current safety state
 */
uint8_t safetyGetState(void);

/**
 * Force safety off (override button)
 * Used when FMU sends FORCE_SAFETY_OFF_MAGIC
 */
void safetyForceOff(void);
