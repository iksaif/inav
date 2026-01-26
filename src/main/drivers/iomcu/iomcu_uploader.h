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

#include "drivers/serial.h"

/**
 * Upload firmware to IOMCU via bootloader
 *
 * @param serial Serial port (UART6) for communication
 * @param firmware Pointer to firmware binary
 * @param size Size of firmware in bytes
 * @return true if upload successful, false otherwise
 */
bool iomcuUploaderUpload(serialPort_t *serial, const uint8_t *firmware, uint32_t size);

#endif // USE_IOMCU
