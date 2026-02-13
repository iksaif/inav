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

#include <stdint.h>
#include <platform.h>
#include "drivers/io.h"
#include "drivers/timer.h"
#include "drivers/bus.h"

// Primary IMU: ICM-42688P on SPI1
BUSDEV_REGISTER_SPI_TAG(busdev_icm42605,    DEVHW_ICM42605,     ICM42605_SPI_BUS,   ICM42605_CS_PIN,    ICM42605_EXTI_PIN,    0, DEVFLAGS_NONE, IMU_ICM42605_ALIGN);

// Secondary IMU: BMI088 (Rev 2 hardware)
BUSDEV_REGISTER_SPI_TAG(busdev_bmi088_gyro, DEVHW_BMI088_GYRO,  BUS_SPI1,           BMI055_GYRO_CS_PIN, BMI055_GYRO_EXTI_PIN, 0, DEVFLAGS_NONE, IMU_BMI055_ALIGN);
BUSDEV_REGISTER_SPI_TAG(busdev_bmi088_acc,  DEVHW_BMI088_ACC,   BUS_SPI1,           BMI055_ACC_CS_PIN,  BMI055_ACC_EXTI_PIN,  0, DEVFLAGS_NONE, IMU_BMI055_ALIGN);

// Secondary IMU: BMI055 (Rev 0/1 hardware) - same pins as BMI088, auto-detect by chip ID
BUSDEV_REGISTER_SPI_TAG(busdev_bmi055_gyro, DEVHW_BMI055_GYRO,  BUS_SPI1,           BMI055_GYRO_CS_PIN, BMI055_GYRO_EXTI_PIN, 0, DEVFLAGS_NONE, IMU_BMI055_ALIGN);
BUSDEV_REGISTER_SPI_TAG(busdev_bmi055_acc,  DEVHW_BMI055_ACC,   BUS_SPI1,           BMI055_ACC_CS_PIN,  BMI055_ACC_EXTI_PIN,  0, DEVFLAGS_NONE, IMU_BMI055_ALIGN);

// FRAM storage on SPI2
BUSDEV_REGISTER_SPI(busdev_ramtron, DEVHW_RAMTRON, RAMTRON_SPI_BUS, RAMTRON_CS_PIN, NONE, DEVFLAGS_NONE, 0);

// PWM timer hardware - 6 or 8 outputs depending on variant + beeper
timerHardware_t timerHardware[] = {
    // PWM outputs - DMA_OPT 0, output indices 0-5 (Mini) or 0-7 (Standard)
    DEF_TIM(TIM1,  CH1, PA8,  TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM1
    DEF_TIM(TIM1,  CH2, PE11, TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM2
    DEF_TIM(TIM1,  CH3, PE13, TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM3
    DEF_TIM(TIM1,  CH4, PE14, TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM4
    DEF_TIM(TIM4,  CH3, PD14, TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM5
    DEF_TIM(TIM4,  CH4, PD15, TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM6
#ifndef PIXHAWK6C_MINI
    DEF_TIM(TIM5,  CH1, PA0,  TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM7 - standard only
    DEF_TIM(TIM5,  CH2, PA1,  TIM_USE_OUTPUT_AUTO, 0, 0),  // PWM8 - standard only
#endif

    // Beeper PWM
    DEF_TIM(TIM3,  CH3, PB0,  TIM_USE_BEEPER, 0, 0),
};

const int timerHardwareCount = sizeof(timerHardware) / sizeof(timerHardware[0]);
