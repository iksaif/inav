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

#include "iomcu_safety.h"

// STM32F103 GPIO definitions
#define RCC_APB2ENR (*(volatile uint32_t*)0x40021018)
#define RCC_APB2ENR_IOPBEN (1 << 3)

#define GPIOB_BASE 0x40010C00
#define GPIOB_CRH  (*(volatile uint32_t*)(GPIOB_BASE + 0x04))
#define GPIOB_IDR  (*(volatile uint32_t*)(GPIOB_BASE + 0x08))
#define GPIOB_ODR  (*(volatile uint32_t*)(GPIOB_BASE + 0x0C))

// Safety button on PB15 (active-low with pull-up)
#define SAFETY_BUTTON_PIN 15

// Debounce parameters
#define DEBOUNCE_TIME_MS 50
#define TOGGLE_PRESS_TIME_MS 1000

// External timing function
extern uint32_t millis(void);

// Safety state
static struct {
    safetyState_t state;
    bool buttonPressed;
    uint32_t pressStartTime;
    uint32_t lastDebounceTime;
    bool forceOff;
} safety;

void safetyInit(void)
{
    // Enable GPIOB clock
    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;

    // Configure PB15 as input with pull-up
    // CNF=10 (input pull-up/down), MODE=00 (input)
    GPIOB_CRH &= ~(0xF << ((15 - 8) * 4));
    GPIOB_CRH |= (0x8 << ((15 - 8) * 4));
    GPIOB_ODR |= (1 << SAFETY_BUTTON_PIN);  // Enable pull-up

    // Initialize state
    safety.state = SAFETY_STATE_SAFE;
    safety.buttonPressed = false;
    safety.pressStartTime = 0;
    safety.lastDebounceTime = 0;
    safety.forceOff = false;
}

void safetyUpdate(void)
{
    uint32_t now = millis();

    // Read button (active-low)
    bool buttonDown = !(GPIOB_IDR & (1 << SAFETY_BUTTON_PIN));

    // Debounce
    if (buttonDown != safety.buttonPressed) {
        safety.lastDebounceTime = now;
    }

    if ((now - safety.lastDebounceTime) > DEBOUNCE_TIME_MS) {
        // Button state is stable
        if (buttonDown && !safety.buttonPressed) {
            // Button just pressed
            safety.buttonPressed = true;
            safety.pressStartTime = now;
        } else if (!buttonDown && safety.buttonPressed) {
            // Button just released
            safety.buttonPressed = false;
            safety.pressStartTime = 0;
        } else if (buttonDown && safety.buttonPressed) {
            // Button held down
            if ((now - safety.pressStartTime) >= TOGGLE_PRESS_TIME_MS) {
                // Long press detected - toggle safety
                if (safety.state == SAFETY_STATE_SAFE) {
                    safety.state = SAFETY_STATE_ARMED;
                } else {
                    safety.state = SAFETY_STATE_SAFE;
                }
                // Reset to prevent multiple toggles
                safety.pressStartTime = now;
            }
        }
    }

    // Force override
    if (safety.forceOff) {
        safety.state = SAFETY_STATE_ARMED;
    }
}

bool safetyIsOff(void)
{
    return safety.state == SAFETY_STATE_ARMED;
}

uint8_t safetyGetState(void)
{
    return (uint8_t)safety.state;
}

void safetyForceOff(void)
{
    safety.forceOff = true;
    safety.state = SAFETY_STATE_ARMED;
}
