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

#include "iomcu_protocol_handler.h"
#include "iomcu_registers.h"
#include "iomcu_protocol.h"
#include "iomcu_main.h"
#include <string.h>

// STM32F103 UART definitions (USART1 @ PA9/PA10 for io-v2)
#define USART1_BASE 0x40013800
#define RCC_APB2ENR (*(volatile uint32_t*)0x40021018)
#define RCC_APB2ENR_USART1EN (1 << 14)
#define RCC_APB2ENR_IOPAEN   (1 << 2)

typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} USART_TypeDef;

#define USART1 ((USART_TypeDef*)USART1_BASE)

// UART status register bits
#define USART_SR_RXNE  (1 << 5)
#define USART_SR_TXE   (1 << 7)
#define USART_SR_TC    (1 << 6)

// UART control register bits
#define USART_CR1_UE   (1 << 13)
#define USART_CR1_TE   (1 << 3)
#define USART_CR1_RE   (1 << 2)

// Baud rate: 1.5 Mbps @ 72MHz
// USARTDIV = 72MHz / 1.5Mbps = 48
#define UART_BRR_VALUE 48

// Packet state machine
typedef enum {
    STATE_WAIT_COUNT_CODE,
    STATE_WAIT_CRC,
    STATE_WAIT_PAGE,
    STATE_WAIT_OFFSET,
    STATE_WAIT_DATA,
} rxState_t;

// Protocol state
static struct {
    rxState_t state;
    uint8_t count;
    uint8_t code;
    uint8_t crc;
    uint8_t page;
    uint8_t offset;
    uint16_t data[22];
    uint8_t dataIndex;
    uint8_t dataBytesRemaining;
    uint32_t lastRxTime;
    uint32_t rxPacketCount;
} protocol;

// Millisecond counter provided by iomcu_main.c

// Timing
extern void delayMicroseconds(uint32_t us);
extern uint32_t millis(void);

void protocolInit(void)
{
    // Enable clocks for USART1 and GPIOA
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;

    // Configure PA9 (TX) as alternate function push-pull, PA10 (RX) as input pull-up
    // GPIO config for STM32F103 (see reference manual)
    volatile uint32_t *GPIOA_CRH = (volatile uint32_t*)0x40010804;
    *GPIOA_CRH &= ~(0xFF << 4);  // Clear PA9/PA10 config
    *GPIOA_CRH |= (0x0B << 4);   // PA9: Alt func push-pull, 50MHz
    *GPIOA_CRH |= (0x08 << 8);   // PA10: Input pull-up

    // Configure USART
    USART1->BRR = UART_BRR_VALUE;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    // Initialize protocol state
    protocol.state = STATE_WAIT_COUNT_CODE;
    protocol.lastRxTime = millis();
}

static void uartWriteByte(uint8_t byte)
{
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = byte;
}

static bool uartReadByte(uint8_t *byte)
{
    if (USART1->SR & USART_SR_RXNE) {
        *byte = (uint8_t)(USART1->DR & 0xFF);
        return true;
    }
    return false;
}

// CRC8 calculation (Dallas/Maxim polynomial 0x07)
static uint8_t crc8(uint8_t crc, const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

static void sendResponse(uint8_t page, uint8_t offset, uint8_t count, const uint16_t *data)
{
    uint8_t count_code = (IOMCU_CODE_READ << 6) | (count & 0x3F);

    // Calculate CRC
    uint8_t calcCrc = 0;
    calcCrc = crc8(calcCrc, &count_code, 1);
    calcCrc = crc8(calcCrc, &page, 1);
    calcCrc = crc8(calcCrc, &offset, 1);
    calcCrc = crc8(calcCrc, (const uint8_t*)data, count * 2);

    // Send packet
    uartWriteByte(count_code);
    uartWriteByte(calcCrc);
    uartWriteByte(page);
    uartWriteByte(offset);

    for (uint8_t i = 0; i < count; i++) {
        uartWriteByte((uint8_t)(data[i] & 0xFF));
        uartWriteByte((uint8_t)(data[i] >> 8));
    }
}

static void processPacket(void)
{
    // Validate CRC
    uint8_t calcCrc = 0;
    uint8_t count_code = (protocol.code << 6) | (protocol.count & 0x3F);
    calcCrc = crc8(calcCrc, &count_code, 1);
    calcCrc = crc8(calcCrc, &protocol.page, 1);
    calcCrc = crc8(calcCrc, &protocol.offset, 1);
    calcCrc = crc8(calcCrc, (const uint8_t*)protocol.data, protocol.count * 2);

    if (calcCrc != protocol.crc) {
        registersIncrementRxErrors();
        return;
    }

    // Update last RX time
    protocol.lastRxTime = millis();
    protocol.rxPacketCount++;

    // Process command
    if (protocol.code == IOMCU_CODE_READ) {
        // Read registers
        uint16_t values[22];
        if (registersRead(protocol.page, protocol.offset, protocol.count, values)) {
            sendResponse(protocol.page, protocol.offset, protocol.count, values);
        }
    } else if (protocol.code == IOMCU_CODE_WRITE) {
        // Write registers
        registersWrite(protocol.page, protocol.offset, protocol.count, protocol.data);
    }
}

void protocolProcess(void)
{
    uint8_t byte;

    while (uartReadByte(&byte)) {
        switch (protocol.state) {
            case STATE_WAIT_COUNT_CODE:
                protocol.code = (byte >> 6) & 0x03;
                protocol.count = byte & 0x3F;
                if (protocol.count > 22) {
                    registersIncrementRxErrors();
                    break;
                }
                protocol.state = STATE_WAIT_CRC;
                break;

            case STATE_WAIT_CRC:
                protocol.crc = byte;
                protocol.state = STATE_WAIT_PAGE;
                break;

            case STATE_WAIT_PAGE:
                protocol.page = byte;
                protocol.state = STATE_WAIT_OFFSET;
                break;

            case STATE_WAIT_OFFSET:
                protocol.offset = byte;
                if (protocol.count > 0) {
                    protocol.dataIndex = 0;
                    protocol.dataBytesRemaining = protocol.count * 2;
                    protocol.state = STATE_WAIT_DATA;
                } else {
                    processPacket();
                    protocol.state = STATE_WAIT_COUNT_CODE;
                }
                break;

            case STATE_WAIT_DATA:
                // Receive data bytes (little-endian uint16)
                if ((protocol.dataIndex & 1) == 0) {
                    // Low byte
                    protocol.data[protocol.dataIndex / 2] = byte;
                } else {
                    // High byte
                    protocol.data[protocol.dataIndex / 2] |= ((uint16_t)byte) << 8;
                }

                protocol.dataIndex++;
                protocol.dataBytesRemaining--;

                if (protocol.dataBytesRemaining == 0) {
                    processPacket();
                    protocol.state = STATE_WAIT_COUNT_CODE;
                }
                break;
        }
    }
}

bool protocolFmuHealthy(void)
{
    return (millis() - protocol.lastRxTime) < IOMCU_WATCHDOG_MS;
}

uint32_t protocolGetLastRxTime(void)
{
    return protocol.lastRxTime;
}

// millis() provided by iomcu_main.c
