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

#include "iomcu_pwm.h"
#include "iomcu_registers.h"
#include <string.h>

// STM32F103 peripheral base addresses
#define TIM1_BASE 0x40012C00
#define TIM4_BASE 0x40000800
#define DMA1_BASE 0x40020000
#define RCC_BASE  0x40021000

// RCC registers
#define RCC_APB2ENR (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR (*(volatile uint32_t*)(RCC_BASE + 0x1C))
#define RCC_AHBENR  (*(volatile uint32_t*)(RCC_BASE + 0x14))

#define RCC_APB2ENR_TIM1EN  (1 << 11)
#define RCC_APB2ENR_IOPAEN  (1 << 2)
#define RCC_APB2ENR_IOPBEN  (1 << 3)
#define RCC_APB1ENR_TIM4EN  (1 << 2)
#define RCC_AHBENR_DMA1EN   (1 << 0)

// Timer registers
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
    volatile uint32_t DCR;
    volatile uint32_t DMAR;
} TIM_TypeDef;

#define TIM1 ((TIM_TypeDef*)TIM1_BASE)
#define TIM4 ((TIM_TypeDef*)TIM4_BASE)

// DMA registers
typedef struct {
    volatile uint32_t CCR;
    volatile uint32_t CNDTR;
    volatile uint32_t CPAR;
    volatile uint32_t CMAR;
    volatile uint32_t reserved;
} DMA_Channel_TypeDef;

typedef struct {
    volatile uint32_t ISR;
    volatile uint32_t IFCR;
    DMA_Channel_TypeDef channels[7];
} DMA_TypeDef;

#define DMA1 ((DMA_TypeDef*)DMA1_BASE)

// Timer control bits
#define TIM_CR1_CEN      (1 << 0)
#define TIM_CR1_ARPE     (1 << 7)
#define TIM_CCER_CC1E    (1 << 0)
#define TIM_CCER_CC2E    (1 << 4)
#define TIM_CCER_CC3E    (1 << 8)
#define TIM_CCER_CC4E    (1 << 12)
#define TIM_BDTR_MOE     (1 << 15)
#define TIM_DIER_UDE     (1 << 8)

// DShot parameters
#define DSHOT_BIT_LENGTH 20
#define DSHOT_FRAME_BITS 18  // 16 data + 2 reset
#define DSHOT_BIT_0      7
#define DSHOT_BIT_1      14

// DShot DMA buffers (one per timer)
static uint16_t dshotDmaBufferTim1[4 * DSHOT_FRAME_BITS];
static uint16_t dshotDmaBufferTim4[4 * DSHOT_FRAME_BITS];

// PWM state
static struct {
    uint8_t mode;
    uint16_t rate;
    bool initialized;
} pwm;

// External references
extern bool safetyIsOff(void);

static void gpioInit(void)
{
    // Enable GPIO clocks
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;

    // Configure TIM1 pins (PA8-PA11) as alternate function push-pull
    volatile uint32_t *GPIOA_CRH = (volatile uint32_t*)0x40010804;
    *GPIOA_CRH &= ~(0xFFFF << 0);  // Clear PA8-PA11
    *GPIOA_CRH |= (0xBBBB << 0);   // Alt func push-pull, 50MHz

    // Configure TIM4 pins (PB6-PB9) as alternate function push-pull
    volatile uint32_t *GPIOB_CRL = (volatile uint32_t*)0x40010C00;
    volatile uint32_t *GPIOB_CRH = (volatile uint32_t*)0x40010C04;
    *GPIOB_CRL &= ~(0xFF << 24);   // Clear PB6-PB7
    *GPIOB_CRL |= (0xBB << 24);    // Alt func push-pull, 50MHz
    *GPIOB_CRH &= ~(0xFF << 0);    // Clear PB8-PB9
    *GPIOB_CRH |= (0xBB << 0);     // Alt func push-pull, 50MHz
}

static void timerInitPwm(TIM_TypeDef *tim, uint16_t period)
{
    // Configure timer for PWM mode
    tim->PSC = 71;  // 72MHz / 72 = 1MHz timer clock
    tim->ARR = period - 1;
    tim->RCR = 0;

    // PWM mode 1, preload enable for all channels
    tim->CCMR1 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);  // CH1/CH2
    tim->CCMR2 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);  // CH3/CH4

    // Enable all channels
    tim->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    // Initialize CCR to neutral (1.5ms)
    tim->CCR1 = 1500;
    tim->CCR2 = 1500;
    tim->CCR3 = 1500;
    tim->CCR4 = 1500;

    // Enable auto-reload preload
    tim->CR1 = TIM_CR1_ARPE;

    // Main output enable (TIM1 only)
    if (tim == TIM1) {
        tim->BDTR = TIM_BDTR_MOE;
    }

    // Enable counter
    tim->CR1 |= TIM_CR1_CEN;
}

static void timerInitDshot(TIM_TypeDef *tim, uint32_t frequency)
{
    // Calculate prescaler for desired DShot bit rate
    // DShot600: 12MHz, DShot300: 6MHz, DShot150: 3MHz
    uint32_t timerFreq = 72000000;  // 72MHz system clock
    uint32_t dshotFreq = frequency;
    uint32_t psc = (timerFreq / (dshotFreq * DSHOT_BIT_LENGTH)) - 1;

    tim->PSC = psc;
    tim->ARR = DSHOT_BIT_LENGTH - 1;
    tim->RCR = 0;

    // PWM mode 1 for all channels
    tim->CCMR1 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);
    tim->CCMR2 = (6 << 4) | (1 << 3) | (6 << 12) | (1 << 11);

    // Enable all channels
    tim->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

    // Initialize to idle (no pulse)
    tim->CCR1 = 0;
    tim->CCR2 = 0;
    tim->CCR3 = 0;
    tim->CCR4 = 0;

    // Enable DMA requests on update
    tim->DIER = TIM_DIER_UDE;

    // Configure DMA burst: 4 registers (CCR1-CCR4), base address = CCR1
    tim->DCR = (3 << 8) | 13;  // DBA=13 (CCR1 offset), DBL=3 (4 transfers)

    // Main output enable (TIM1 only)
    if (tim == TIM1) {
        tim->BDTR = TIM_BDTR_MOE;
    }

    // Don't enable counter yet - will be triggered by DMA
}

static void dmaInit(void)
{
    // Enable DMA1 clock
    RCC_AHBENR |= RCC_AHBENR_DMA1EN;

    // Configure DMA1 Channel 5 for TIM1 (UP event)
    DMA1->channels[4].CCR = 0;  // Disable channel
    DMA1->channels[4].CPAR = (uint32_t)&TIM1->DMAR;
    DMA1->channels[4].CMAR = (uint32_t)dshotDmaBufferTim1;
    DMA1->channels[4].CNDTR = 4 * DSHOT_FRAME_BITS;
    // Memory increment, 16-bit, medium priority, memory-to-peripheral
    DMA1->channels[4].CCR = (1 << 7) | (1 << 10) | (1 << 8) | (1 << 4);

    // Configure DMA1 Channel 1 for TIM4 (UP event)
    DMA1->channels[0].CCR = 0;  // Disable channel
    DMA1->channels[0].CPAR = (uint32_t)&TIM4->DMAR;
    DMA1->channels[0].CMAR = (uint32_t)dshotDmaBufferTim4;
    DMA1->channels[0].CNDTR = 4 * DSHOT_FRAME_BITS;
    // Memory increment, 16-bit, medium priority, memory-to-peripheral
    DMA1->channels[0].CCR = (1 << 7) | (1 << 10) | (1 << 8) | (1 << 4);
}

static uint16_t dshotEncode(uint16_t throttle, bool telemetry)
{
    // DShot frame: 11-bit throttle, 1-bit telemetry, 4-bit CRC
    if (throttle > 2047) throttle = 2047;

    uint16_t packet = (throttle << 1) | (telemetry ? 1 : 0);

    // Calculate CRC (XOR of three 4-bit nibbles)
    uint8_t crc = 0;
    uint16_t temp = packet;
    for (int i = 0; i < 3; i++) {
        crc ^= (temp & 0xF);
        temp >>= 4;
    }

    return (packet << 4) | (crc & 0xF);
}

static void dshotPrepareDma(uint16_t *buffer, const uint16_t *values, uint8_t count)
{
    for (uint8_t ch = 0; ch < count; ch++) {
        uint16_t frame = dshotEncode(values[ch], false);

        // Encode 16-bit frame into DMA buffer
        for (uint8_t bit = 0; bit < 16; bit++) {
            uint8_t bitVal = (frame >> (15 - bit)) & 1;
            buffer[ch * DSHOT_FRAME_BITS + bit] = bitVal ? DSHOT_BIT_1 : DSHOT_BIT_0;
        }

        // Add reset bits (2 x low)
        buffer[ch * DSHOT_FRAME_BITS + 16] = 0;
        buffer[ch * DSHOT_FRAME_BITS + 17] = 0;
    }
}

static void dshotTrigger(TIM_TypeDef *tim, DMA_Channel_TypeDef *dma, uint16_t *buffer, uint8_t count)
{
    // Stop timer
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->CNT = 0;

    // Reset DMA
    dma->CCR &= ~(1 << 0);  // Disable
    dma->CNDTR = count * DSHOT_FRAME_BITS;
    dma->CCR |= (1 << 0);   // Enable

    // Start timer
    tim->CR1 |= TIM_CR1_CEN;
}

void pwmInit(void)
{
    // Enable peripheral clocks
    RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;
    RCC_APB1ENR |= RCC_APB1ENR_TIM4EN;

    // Initialize GPIOs
    gpioInit();

    // Initialize DMA for DShot
    dmaInit();

    // Default to standard PWM mode at 50Hz
    pwm.mode = PWM_MODE_STANDARD;
    pwm.rate = 50;

    // Initialize timers in PWM mode
    uint16_t period = 20000;  // 20ms = 50Hz
    timerInitPwm(TIM1, period);
    timerInitPwm(TIM4, period);

    pwm.initialized = true;
}

void pwmSetMode(uint8_t mode)
{
    if (mode == pwm.mode) return;

    pwm.mode = mode;

    // Reconfigure timers based on mode
    switch (mode) {
        case PWM_MODE_STANDARD:
            timerInitPwm(TIM1, 20000 / (pwm.rate / 50));  // Scale period
            timerInitPwm(TIM4, 20000 / (pwm.rate / 50));
            break;

        case PWM_MODE_ONESHOT125:
            timerInitPwm(TIM1, 250);  // 125-250us range
            timerInitPwm(TIM4, 250);
            break;

        case PWM_MODE_DSHOT150:
            timerInitDshot(TIM1, 3000000);  // 3MHz
            timerInitDshot(TIM4, 3000000);
            break;

        case PWM_MODE_DSHOT300:
            timerInitDshot(TIM1, 6000000);  // 6MHz
            timerInitDshot(TIM4, 6000000);
            break;

        case PWM_MODE_DSHOT600:
            timerInitDshot(TIM1, 12000000);  // 12MHz
            timerInitDshot(TIM4, 12000000);
            break;
    }
}

void pwmSetRate(uint16_t rate)
{
    if (rate < 50) rate = 50;
    if (rate > 400) rate = 400;

    pwm.rate = rate;

    // Update period if in standard PWM mode
    if (pwm.mode == PWM_MODE_STANDARD) {
        uint16_t period = 1000000 / rate;  // Convert Hz to us
        TIM1->ARR = period - 1;
        TIM4->ARR = period - 1;
    }
}

void pwmUpdate(void)
{
    if (!pwm.initialized) return;

    // Check safety
    if (!safetyIsOff()) {
        pwmDisableAll();
        return;
    }

    // Get values from registers
    uint16_t pwmValues[8];
    for (uint8_t i = 0; i < 8; i++) {
        if (pwm.mode >= PWM_MODE_DSHOT150) {
            pwmValues[i] = registersGetDshotValue(i);
        } else {
            pwmValues[i] = registersGetPwmValue(i);
        }
    }

    // Update outputs based on mode
    if (pwm.mode >= PWM_MODE_DSHOT150) {
        // DShot mode - use DMA
        dshotPrepareDma(dshotDmaBufferTim1, &pwmValues[0], 4);
        dshotPrepareDma(dshotDmaBufferTim4, &pwmValues[4], 4);

        dshotTrigger(TIM1, &DMA1->channels[4], dshotDmaBufferTim1, 4);
        dshotTrigger(TIM4, &DMA1->channels[0], dshotDmaBufferTim4, 4);
    } else {
        // Standard PWM or OneShot - direct CCR write
        TIM1->CCR1 = pwmValues[0];
        TIM1->CCR2 = pwmValues[1];
        TIM1->CCR3 = pwmValues[2];
        TIM1->CCR4 = pwmValues[3];

        TIM4->CCR1 = pwmValues[4];
        TIM4->CCR2 = pwmValues[5];
        TIM4->CCR3 = pwmValues[6];
        TIM4->CCR4 = pwmValues[7];
    }
}

void pwmDisableAll(void)
{
    // Set all outputs to neutral/idle
    if (pwm.mode >= PWM_MODE_DSHOT150) {
        // DShot idle (zero throttle)
        TIM1->CCR1 = 0;
        TIM1->CCR2 = 0;
        TIM1->CCR3 = 0;
        TIM1->CCR4 = 0;

        TIM4->CCR1 = 0;
        TIM4->CCR2 = 0;
        TIM4->CCR3 = 0;
        TIM4->CCR4 = 0;
    } else {
        // PWM neutral (1.5ms)
        TIM1->CCR1 = 1500;
        TIM1->CCR2 = 1500;
        TIM1->CCR3 = 1500;
        TIM1->CCR4 = 1500;

        TIM4->CCR1 = 1500;
        TIM4->CCR2 = 1500;
        TIM4->CCR3 = 1500;
        TIM4->CCR4 = 1500;
    }
}
