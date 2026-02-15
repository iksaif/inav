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

#include "iomcu_main.h"
#include "iomcu_registers.h"
#include "iomcu_protocol_handler.h"
#include "iomcu_safety.h"
#include "iomcu_pwm.h"

// STM32F103 system control
#define SCB_AIRCR (*(volatile uint32_t*)0xE000ED0C)
#define AIRCR_VECTKEY_MASK 0x05FA0000

// SysTick timer for millisecond counter
#define SYSTICK_BASE 0xE000E010
#define SYSTICK_CSR  (*(volatile uint32_t*)(SYSTICK_BASE + 0x00))
#define SYSTICK_RVR  (*(volatile uint32_t*)(SYSTICK_BASE + 0x04))
#define SYSTICK_CVR  (*(volatile uint32_t*)(SYSTICK_BASE + 0x08))

#define SYSTICK_CSR_ENABLE    (1 << 0)
#define SYSTICK_CSR_TICKINT   (1 << 1)
#define SYSTICK_CSR_CLKSOURCE (1 << 2)

// System state
static volatile uint32_t systemMillis = 0;
static bool failsafeActive = false;
static uint32_t lastFmuRxTime = 0;

// Firmware CRC (calculated at build time, stored in flash)
extern uint32_t _firmware_crc;

void SysTick_Handler(void)
{
    systemMillis++;
}

static void systemClockInit(void)
{
    // STM32F103 should already be configured by bootloader
    // Assuming 72MHz system clock (HSE + PLL)

    // Configure SysTick for 1ms interrupts
    SYSTICK_RVR = 72000 - 1;  // 72MHz / 72000 = 1kHz
    SYSTICK_CVR = 0;
    SYSTICK_CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE;
}

static void calculateFirmwareCrc(void)
{
    // For now, use a placeholder CRC
    // In actual implementation, this should be calculated over the firmware image
    // and stored in a known location by the build process
    uint32_t crc = 0xDEADBEEF;  // Placeholder

    registersSetFirmwareCrc(crc);
}

bool fmuIsHealthy(void)
{
    return protocolFmuHealthy();
}

bool isFailsafe(void)
{
    return failsafeActive;
}

uint32_t millis(void)
{
    return systemMillis;
}

void delayMicroseconds(uint32_t us)
{
    // Simple delay loop (approximate, depends on compiler optimization)
    // At 72MHz, ~72 cycles per microsecond
    volatile uint32_t cycles = us * 18;  // Rough calibration
    while (cycles--) {
        __asm__ volatile ("nop");
    }
}

void rebootToBootloader(void)
{
    // Disable interrupts
    __asm__ volatile ("cpsid i");

    // System reset via AIRCR
    SCB_AIRCR = AIRCR_VECTKEY_MASK | (1 << 2);

    // Wait for reset
    while (1);
}

static void checkRebootRequest(void)
{
    if (registersCheckRebootRequest()) {
        registersClearRebootRequest();
        // Delay to allow FMU to receive ACK
        for (volatile int i = 0; i < 1000000; i++);
        rebootToBootloader();
    }
}

static void updateFailsafe(void)
{
    uint32_t now = millis();

    // Check FMU watchdog timeout
    if (!fmuIsHealthy()) {
        if (!failsafeActive) {
            failsafeActive = true;
            // In failsafe: hold last received PWM values (already in registers)
        }
    } else {
        failsafeActive = false;
        lastFmuRxTime = now;
    }
}

static void updateOutputMode(void)
{
    static uint8_t lastMode = 0xFF;
    static uint16_t lastRate = 0;

    uint8_t mode = registersGetOutputMode();
    uint16_t rate = registersGetPwmRate();

    if (mode != lastMode) {
        pwmSetMode(mode);
        lastMode = mode;
    }

    if (rate != lastRate) {
        pwmSetRate(rate);
        lastRate = rate;
    }
}

int main(void)
{
    // Initialize system clock and SysTick
    systemClockInit();

    // Initialize all subsystems
    registersInit();
    protocolInit();
    safetyInit();
    pwmInit();

    // Calculate and store firmware CRC
    calculateFirmwareCrc();

    // Update status once before main loop
    registersUpdateStatus();

    // Timing for periodic tasks
    uint32_t lastSafetyUpdate = 0;
    uint32_t lastPwmUpdate = 0;
    uint32_t lastStatusUpdate = 0;

    // Main loop - cooperative multitasking
    while (1) {
        uint32_t now = millis();

        // High priority: Process incoming serial packets
        protocolProcess();

        // Check for reboot request
        checkRebootRequest();

        // Update failsafe state
        updateFailsafe();

        // Update output mode if changed
        updateOutputMode();

        // Update PWM outputs (2-20ms depending on rate)
        if (now - lastPwmUpdate >= 5) {
            pwmUpdate();
            lastPwmUpdate = now;
        }

        // Update safety button (10ms)
        if (now - lastSafetyUpdate >= 10) {
            safetyUpdate();
            lastSafetyUpdate = now;
        }

        // Update status registers (50ms)
        if (now - lastStatusUpdate >= 50) {
            registersUpdateStatus();
            lastStatusUpdate = now;
        }
    }

    return 0;  // Never reached
}
