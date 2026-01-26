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
 * Main firmware entry point
 * Called after hardware initialization
 */
int main(void);

/**
 * Check if FMU communication is healthy
 */
bool fmuIsHealthy(void);

/**
 * Check if in failsafe mode
 */
bool isFailsafe(void);

/**
 * System millisecond counter
 */
uint32_t millis(void);

/**
 * Delay microseconds (blocking)
 */
void delayMicroseconds(uint32_t us);

/**
 * Reboot to bootloader
 */
void rebootToBootloader(void);
