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

#include "drivers/flash.h"

#define RAMTRON_PAGESIZE 256

bool ramtron_init(int flashNumToUse);
bool ramtron_isReady(void);
bool ramtron_waitForReady(timeMs_t timeoutMillis);
void ramtron_eraseSector(uint32_t address);
void ramtron_eraseCompletely(void);
uint32_t ramtron_pageProgram(uint32_t address, const uint8_t *data, int length);
int ramtron_readBytes(uint32_t address, uint8_t *buffer, int length);
const flashGeometry_t *ramtron_getGeometry(void);
