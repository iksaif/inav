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

#include "platform.h"

#ifdef USE_IOMCU

#include "iomcu_uploader.h"
#include "iomcu_protocol.h"

#include "drivers/serial.h"
#include "drivers/time.h"
#include "drivers/system.h"

#include "common/crc.h"
#include "common/log.h"

// Bootloader communication helpers
static bool uploaderSendByte(serialPort_t *serial, uint8_t byte)
{
    serialWrite(serial, byte);
    return true;
}

static bool uploaderReadByte(serialPort_t *serial, uint8_t *byte, uint32_t timeoutMs)
{
    uint32_t startTime = millis();
    while ((millis() - startTime) < timeoutMs) {
        if (serialRxBytesWaiting(serial) > 0) {
            *byte = serialRead(serial);
            return true;
        }
    }
    return false;
}

static bool uploaderSync(serialPort_t *serial)
{
    // Send PROTO_GET_SYNC, expect PROTO_INSYNC + PROTO_OK
    uploaderSendByte(serial, PROTO_GET_SYNC);
    uploaderSendByte(serial, PROTO_EOC);

    uint8_t resp1, resp2;
    if (!uploaderReadByte(serial, &resp1, 40)) return false;
    if (!uploaderReadByte(serial, &resp2, 40)) return false;

    return (resp1 == PROTO_INSYNC && resp2 == PROTO_OK);
}

static bool uploaderGetInfo(serialPort_t *serial, uint8_t infoType, uint32_t *value)
{
    uploaderSendByte(serial, PROTO_GET_DEVICE);
    uploaderSendByte(serial, infoType);
    uploaderSendByte(serial, PROTO_EOC);

    uint8_t resp;
    if (!uploaderReadByte(serial, &resp, 100)) return false;
    if (resp != PROTO_INSYNC) return false;

    // Read 4-byte value (little-endian)
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) {
        if (!uploaderReadByte(serial, &bytes[i], 100)) return false;
    }
    *value = bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);

    if (!uploaderReadByte(serial, &resp, 100)) return false;
    return (resp == PROTO_OK);
}

static bool uploaderErase(serialPort_t *serial)
{
    uploaderSendByte(serial, PROTO_CHIP_ERASE);
    uploaderSendByte(serial, PROTO_EOC);

    uint8_t resp1, resp2;
    if (!uploaderReadByte(serial, &resp1, 10000)) return false;  // 10s timeout for erase
    if (!uploaderReadByte(serial, &resp2, 100)) return false;

    return (resp1 == PROTO_INSYNC && resp2 == PROTO_OK);
}

static bool uploaderProgram(serialPort_t *serial, uint32_t address, const uint8_t *data, uint8_t len)
{
    (void)address;  // Address not needed for PROG_MULTI protocol

    if (len > PROG_MULTI_MAX) return false;

    uploaderSendByte(serial, PROTO_PROG_MULTI);
    uploaderSendByte(serial, len);

    for (uint8_t i = 0; i < len; i++) {
        uploaderSendByte(serial, data[i]);
    }

    uploaderSendByte(serial, PROTO_EOC);

    uint8_t resp1, resp2;
    if (!uploaderReadByte(serial, &resp1, 1000)) return false;
    if (!uploaderReadByte(serial, &resp2, 100)) return false;

    return (resp1 == PROTO_INSYNC && resp2 == PROTO_OK);
}

// TODO: Re-enable when CRC32 verification is implemented
__attribute__((unused))
static bool uploaderGetCrc(serialPort_t *serial, uint32_t *crc)
{
    uploaderSendByte(serial, PROTO_GET_CRC);
    uploaderSendByte(serial, PROTO_EOC);

    uint8_t resp;
    if (!uploaderReadByte(serial, &resp, 1000)) return false;
    if (resp != PROTO_INSYNC) return false;

    // Read 4-byte CRC (little-endian)
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) {
        if (!uploaderReadByte(serial, &bytes[i], 100)) return false;
    }
    *crc = bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);

    if (!uploaderReadByte(serial, &resp, 100)) return false;
    return (resp == PROTO_OK);
}

static bool uploaderReboot(serialPort_t *serial)
{
    uploaderSendByte(serial, PROTO_REBOOT);
    delay(200);
    uploaderSendByte(serial, PROTO_EOC);
    delay(200);

    // IOMCU should now be rebooting to application
    return true;
}

bool iomcuUploaderUpload(serialPort_t *serial, const uint8_t *firmware, uint32_t size)
{
    if (!serial || !firmware || size == 0 || size > IOMCU_FLASH_SIZE) {
        return false;
    }

    // Switch to bootloader baud rate
    serialSetBaudRate(serial, IOMCU_BOOTLOADER_BAUD);

    // Drain any pending data
    while (serialRxBytesWaiting(serial) > 0) {
        serialRead(serial);
    }

    // Try to sync with bootloader (15 attempts)
    LOG_INFO(IOMCU, "Syncing with bootloader...");
    bool synced = false;
    for (int i = 0; i < 15; i++) {
        if (uploaderSync(serial)) {
            synced = true;
            break;
        }
        delay(10);
    }

    if (!synced) {
        LOG_ERROR(IOMCU, "Failed to sync with bootloader");
        serialSetBaudRate(serial, IOMCU_BAUD_RATE);
        return false;
    }

    LOG_INFO(IOMCU, "Bootloader synced");

    // Get bootloader info
    uint32_t blRev;
    if (!uploaderGetInfo(serial, INFO_BL_REV, &blRev)) {
        LOG_ERROR(IOMCU, "Failed to get bootloader revision");
        serialSetBaudRate(serial, IOMCU_BAUD_RATE);
        return false;
    }

    if (blRev > BL_REV) {
        LOG_ERROR(IOMCU, "Unsupported bootloader revision: %lu", blRev);
        serialSetBaudRate(serial, IOMCU_BAUD_RATE);
        return false;
    }

    LOG_INFO(IOMCU, "Bootloader rev: %lu", blRev);

    // TODO: Implement CRC32 verification to skip upload if firmware matches
    // For now, always upload firmware
    (void)blRev;  // Unused for now

    // Erase flash
    LOG_INFO(IOMCU, "Erasing flash...");
    if (!uploaderErase(serial)) {
        LOG_ERROR(IOMCU, "Flash erase failed");
        serialSetBaudRate(serial, IOMCU_BAUD_RATE);
        return false;
    }

    LOG_INFO(IOMCU, "Flash erased");

    // Program firmware in chunks
    LOG_INFO(IOMCU, "Programming firmware (%lu bytes)...", size);
    uint32_t offset = 0;
    while (offset < size) {
        uint8_t chunkSize = (size - offset) > PROG_MULTI_MAX ? PROG_MULTI_MAX : (size - offset);

        if (!uploaderProgram(serial, offset, &firmware[offset], chunkSize)) {
            LOG_ERROR(IOMCU, "Programming failed at offset %lu", offset);
            serialSetBaudRate(serial, IOMCU_BAUD_RATE);
            return false;
        }

        offset += chunkSize;

        // Progress indication every 4KB
        if ((offset % 4096) == 0 || offset >= size) {
            LOG_INFO(IOMCU, "Programmed %lu / %lu bytes", offset, size);
        }
    }

    // TODO: Implement CRC32 firmware verification
    // For now, skip verification step
    LOG_INFO(IOMCU, "Firmware upload complete (verification skipped)");

    // Reboot to application
    LOG_INFO(IOMCU, "Rebooting IOMCU...");
    uploaderReboot(serial);

    // Restore runtime baud rate
    serialSetBaudRate(serial, IOMCU_BAUD_RATE);

    LOG_INFO(IOMCU, "Firmware upload complete");
    return true;
}

#endif // USE_IOMCU
