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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#ifdef USE_FLASH_RAMTRON

#include "flash_ramtron.h"
#include "drivers/io.h"
#include "drivers/bus.h"
#include "drivers/time.h"

// RAMTRON/FRAM opcodes
#define RAMTRON_OPCODE_WREN     0x06  // Write Enable
#define RAMTRON_OPCODE_WRDI     0x04  // Write Disable
#define RAMTRON_OPCODE_RDSR     0x05  // Read Status Register
#define RAMTRON_OPCODE_WRSR     0x01  // Write Status Register
#define RAMTRON_OPCODE_READ     0x03  // Read Data Bytes
#define RAMTRON_OPCODE_WRITE    0x02  // Write Data Bytes
#define RAMTRON_OPCODE_RDID     0x9F  // Read Device ID
#define RAMTRON_OPCODE_SLEEP    0xB9  // Enter Sleep Mode
#define RAMTRON_OPCODE_SNR      0xC3  // Read Serial Number

// Status register bits
#define RAMTRON_STATUS_WEL      0x02  // Write Enable Latch
#define RAMTRON_STATUS_BP0      0x04  // Block Protect bit 0
#define RAMTRON_STATUS_BP1      0x08  // Block Protect bit 1
#define RAMTRON_STATUS_WPEN     0x80  // Write Protect Enable

// FRAM chip identification structure
typedef struct {
    uint8_t id1;           // First ID byte (memory type)
    uint8_t id2;           // Second ID byte (density/version)
    uint16_t sizeKB;       // Size in kilobytes
    uint8_t addrLen;       // Address length in bytes (2 or 3)
    const char *name;      // Chip name for debugging
} ramtronDeviceInfo_t;

// Known FRAM chip database (from ArduPilot and datasheets)
// Format: ID1, ID2, Size(KB), AddrLen, Name
static const ramtronDeviceInfo_t ramtronDevices[] = {
    // Cypress/Infineon FM25V series
    { 0x21, 0x00,  16, 2, "FM25V01" },      // 128 Kbit
    { 0x21, 0x08,  16, 2, "FM25V01A" },     // 128 Kbit
    { 0x22, 0x00,  32, 2, "FM25V02" },      // 256 Kbit
    { 0x22, 0x08,  32, 2, "FM25V02A" },     // 256 Kbit (Pixhawk 6C likely uses this)
    { 0x22, 0x48,  32, 2, "FM25V02A-ET" },  // 256 Kbit Extended Temp
    { 0x22, 0x01,  32, 2, "FM25VN02" },     // 256 Kbit
    { 0x23, 0x00,  64, 2, "FM25V05" },      // 512 Kbit
    { 0x23, 0x01,  64, 2, "FM25VN05" },     // 512 Kbit
    { 0x24, 0x00, 128, 3, "FM25V10" },      // 1 Mbit
    { 0x24, 0x01, 128, 3, "FM25VN10" },     // 1 Mbit
    { 0x25, 0x08, 256, 3, "FM25V20A" },     // 2 Mbit
    { 0x26, 0x08, 512, 3, "CY15B104Q" },    // 4 Mbit

    // Fujitsu MB85RS series
    { 0x27, 0x03, 128, 3, "MB85RS1MT" },    // 1 Mbit
    { 0x05, 0x09,  32, 2, "MB85RS256B" },   // 256 Kbit
    { 0x24, 0x03,  16, 2, "MB85RS128TY" },  // 128 Kbit
    { 0x25, 0x03,  32, 2, "MB85RS256TY" },  // 256 Kbit
};

#define RAMTRON_NUM_DEVICES (sizeof(ramtronDevices) / sizeof(ramtronDevices[0]))

static flashGeometry_t geometry = {
    .pageSize = RAMTRON_PAGESIZE
};

static busDevice_t *busDev = NULL;
static bool isInitialized = false;
static uint8_t detectedDeviceIndex = 0xFF;  // Index into ramtronDevices table

/**
 * Read device ID and detect FRAM chip
 * Cypress/Infineon RDID format: [0x7F] [0x7F] [0x7F] [0x7F] [0x7F] [0x7F] [0xC2] [ID1] [ID2]
 * We need to read enough bytes to get past the continuation codes (0x7F)
 */
static bool ramtron_detectDevice(void)
{
    // Read RDID response (9 bytes for Cypress chips with continuation codes)
    uint8_t txBuf[10] = { RAMTRON_OPCODE_RDID, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t rxBuf[10];

    busTransfer(busDev, txBuf, rxBuf, sizeof(txBuf));

    // For Cypress/Infineon chips:
    // Skip command echo (rxBuf[0])
    // Skip continuation codes (0x7F) - typically 6 bytes
    // Manufacturer ID should be 0xC2 for Cypress (rxBuf[7])
    // ID1 is at rxBuf[8] (memory type)
    // ID2 is at rxBuf[9] (density code)

    uint8_t id1 = rxBuf[8];
    uint8_t id2 = rxBuf[9];

    // Try to match against known devices
    for (uint8_t i = 0; i < RAMTRON_NUM_DEVICES; i++) {
        if (ramtronDevices[i].id1 == id1 && ramtronDevices[i].id2 == id2) {
            detectedDeviceIndex = i;
            return true;
        }
    }

    // Device not found in our database
    detectedDeviceIndex = 0xFF;
    return false;
}

/**
 * Configure geometry based on detected device
 */
static void ramtron_setGeometry(void)
{
    if (detectedDeviceIndex >= RAMTRON_NUM_DEVICES) {
        // Should not happen - device should have been detected
        return;
    }

    const ramtronDeviceInfo_t *device = &ramtronDevices[detectedDeviceIndex];
    uint32_t totalSize = device->sizeKB * 1024;

    geometry.totalSize = totalSize;
    geometry.sectorSize = 4096;  // Use 4K sectors for compatibility
    geometry.sectors = totalSize / geometry.sectorSize;
    geometry.pagesPerSector = geometry.sectorSize / geometry.pageSize;
    geometry.flashType = FLASH_TYPE_NOR;
}

bool ramtron_init(int flashNumToUse)
{
    UNUSED(flashNumToUse);

    if (isInitialized) {
        return true;
    }

    busDev = busDeviceInit(BUSTYPE_SPI, DEVHW_RAMTRON, 0, OWNER_FLASH);
    if (busDev == NULL) {
        return false;
    }

    // Detect and identify FRAM chip
    if (!ramtron_detectDevice()) {
        return false;
    }

    // Set up geometry based on detected device
    ramtron_setGeometry();

    isInitialized = true;
    return true;
}

/**
 * Get the address length for the detected device
 */
static uint8_t ramtron_getAddressLength(void)
{
    if (detectedDeviceIndex >= RAMTRON_NUM_DEVICES) {
        return 3;  // Default to 3-byte addressing
    }
    return ramtronDevices[detectedDeviceIndex].addrLen;
}

bool ramtron_isReady(void)
{
    // FRAM is always ready (no erase cycles, no write delays)
    return isInitialized;
}

bool ramtron_waitForReady(timeMs_t timeoutMillis)
{
    UNUSED(timeoutMillis);
    // FRAM is always ready
    return isInitialized;
}

/**
 * Internal function that performs the actual sector erase by writing 0xFF
 */
static void ramtron_eraseSectorInternal(uint32_t address)
{
    if (!isInitialized) {
        return;
    }

    // For FRAM, "erase" means writing all 0xFF to simulate erased state
    // Write in chunks to avoid large SPI transfers
    uint8_t eraseBuf[RAMTRON_PAGESIZE];
    memset(eraseBuf, 0xFF, sizeof(eraseBuf));

    uint32_t sectorStart = (address / geometry.sectorSize) * geometry.sectorSize;
    uint32_t remaining = geometry.sectorSize;
    uint32_t currentAddr = sectorStart;

    while (remaining > 0) {
        uint32_t chunkSize = (remaining < RAMTRON_PAGESIZE) ? remaining : RAMTRON_PAGESIZE;
        ramtron_pageProgram(currentAddr, eraseBuf, chunkSize);
        currentAddr += chunkSize;
        remaining -= chunkSize;
    }
}

void ramtron_eraseSector(uint32_t address)
{
    // NOTE: It's unclear if this should be a no-op or not.
    // FRAM doesn't require erasing before writes (can overwrite directly),
    // but when explicitly called (e.g., via flashPartitionErase), it may be
    // expected to reset the sector to 0xFF to match traditional flash behavior.
    // Current implementation calls the internal erase function.
    // To make this a no-op, simply comment out the line below.

    ramtron_eraseSectorInternal(address);
}

void ramtron_eraseCompletely(void)
{
    if (!isInitialized) {
        return;
    }

    // NOTE: This is explicitly called by user actions (CLI, MSP, CMS menu),
    // so it should perform the erase operation rather than being a no-op.
    // For FRAM, "erase" means writing all 0xFF to entire flash to simulate
    // the erased state of traditional flash memory.

    // Erase sector by sector for better progress tracking
    for (uint32_t sector = 0; sector < geometry.sectors; sector++) {
        ramtron_eraseSectorInternal(sector * geometry.sectorSize);
    }
}

uint32_t ramtron_pageProgram(uint32_t address, const uint8_t *data, int length)
{
    if (!isInitialized || length <= 0) {
        return 0;
    }

    // Enable write
    uint8_t cmd = RAMTRON_OPCODE_WREN;
    busTransfer(busDev, &cmd, NULL, 1);

    // Write data - FRAM allows direct byte writes, no page programming needed
    uint8_t addrLen = ramtron_getAddressLength();
    uint8_t txBuf[1 + 3 + length];  // cmd + max 3 addr bytes + data
    uint8_t headerLen;

    txBuf[0] = RAMTRON_OPCODE_WRITE;

    if (addrLen == 3) {
        // 3-byte addressing (for chips >= 128KB)
        txBuf[1] = (address >> 16) & 0xFF;
        txBuf[2] = (address >> 8) & 0xFF;
        txBuf[3] = address & 0xFF;
        headerLen = 4;
    } else {
        // 2-byte addressing (for chips <= 64KB)
        txBuf[1] = (address >> 8) & 0xFF;
        txBuf[2] = address & 0xFF;
        headerLen = 3;
    }

    memcpy(&txBuf[headerLen], data, length);
    busTransfer(busDev, txBuf, NULL, headerLen + length);

    // No need to wait - FRAM writes are instantaneous
    return length;
}

int ramtron_readBytes(uint32_t address, uint8_t *buffer, int length)
{
    if (!isInitialized || length <= 0) {
        return 0;
    }

    uint8_t addrLen = ramtron_getAddressLength();
    uint8_t txBuf[4];  // cmd + max 3 addr bytes
    uint8_t headerLen;

    txBuf[0] = RAMTRON_OPCODE_READ;

    if (addrLen == 3) {
        // 3-byte addressing (for chips >= 128KB)
        txBuf[1] = (address >> 16) & 0xFF;
        txBuf[2] = (address >> 8) & 0xFF;
        txBuf[3] = address & 0xFF;
        headerLen = 4;
    } else {
        // 2-byte addressing (for chips <= 64KB)
        txBuf[1] = (address >> 8) & 0xFF;
        txBuf[2] = address & 0xFF;
        headerLen = 3;
    }

    // For reads, we need to clock out the command and address,
    // then clock in the data
    busTransferDescriptor_t transfers[2] = {
        { .txBuf = txBuf, .rxBuf = NULL, .length = headerLen },
        { .txBuf = NULL, .rxBuf = buffer, .length = length }
    };

    busTransferMultiple(busDev, transfers, 2);

    return length;
}

const flashGeometry_t *ramtron_getGeometry(void)
{
    return &geometry;
}

#endif // USE_FLASH_RAMTRON
