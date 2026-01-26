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
 * Initialize PWM timers and DMA
 */
void pwmInit(void);

/**
 * Update PWM outputs based on current mode and register values
 * Call this from main loop when outputs need updating
 */
void pwmUpdate(void);

/**
 * Set output mode (standard PWM, OneShot, DShot)
 * @param mode Output mode (PWM_MODE_*)
 */
void pwmSetMode(uint8_t mode);

/**
 * Set PWM rate in Hz (for standard PWM mode)
 * @param rate Update rate (50-400 Hz)
 */
void pwmSetRate(uint16_t rate);

/**
 * Disable all PWM outputs (safety)
 */
void pwmDisableAll(void);
