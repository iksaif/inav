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

#define TARGET_BOARD_IDENTIFIER "PH6C"
#define USBD_PRODUCT_STRING     "Pixhawk6C"

// LED indicators (active low)
#define LED0                    PD10    // Red LED
#define LED1                    PD11    // Blue LED

// Beeper
#define BEEPER                  PB0
#define BEEPER_INVERTED

// UARTs - 7 external + VCP + IOMCU = 9 serial ports
#define USE_VCP

#define USE_UART1               // GPS1
#define UART1_TX_PIN            PB6
#define UART1_RX_PIN            PA10

#define USE_UART2               // Telem3
#define UART2_TX_PIN            PD5
#define UART2_RX_PIN            PA3

#define USE_UART3               // Debug
#define UART3_TX_PIN            PD8
#define UART3_RX_PIN            PD9

#define USE_UART5               // Telem2
#define UART5_TX_PIN            PC12
#define UART5_RX_PIN            PD2

#define USE_UART6               // IOMCU (internal coprocessor)
#define UART6_TX_PIN            PC6
#define UART6_RX_PIN            PC7

#define USE_UART7               // Telem1 (with flow control)
#define UART7_TX_PIN            PE8
#define UART7_RX_PIN            PE7
#define UART7_RTS_PIN           PE9
#define UART7_CTS_PIN           PE10

#define USE_UART8               // GPS2
#define UART8_TX_PIN            PE1
#define UART8_RX_PIN            PE0

#define SERIAL_PORT_COUNT       9

// I2C buses
#define USE_I2C
#define USE_I2C_DEVICE_1        // External I2C bus 1
#define I2C1_SCL                PB8
#define I2C1_SDA                PB7

#define USE_I2C_DEVICE_2        // External I2C bus 2
#define I2C2_SCL                PB10
#define I2C2_SDA                PB11

#define USE_I2C_DEVICE_4        // Internal I2C bus
#define I2C4_SCL                PD12
#define I2C4_SDA                PD13

// Barometers on internal I2C4
#define USE_BARO
#define BARO_I2C_BUS            BUS_I2C4
#define USE_BARO_MS5611         // Two MS5611 at 0x76 and 0x77
#define USE_BARO_BMP388         // Alternative BMP388 at 0x77
#define USE_BARO_DPS310
#define USE_BARO_SPL06

// Magnetometer on internal I2C4
#define USE_MAG
#define MAG_I2C_BUS             BUS_I2C4
#define USE_MAG_ALL             // Enable all magnetometer drivers for flexibility

// SPI buses
#define USE_SPI

// SPI1 for IMU sensors
#define USE_SPI_DEVICE_1
#define SPI1_SCK_PIN            PA5
#define SPI1_MISO_PIN           PA6
#define SPI1_MOSI_PIN           PA7

// SPI2 for FRAM storage
#define USE_SPI_DEVICE_2
#define SPI2_SCK_PIN            PD3
#define SPI2_MISO_PIN           PC2
#define SPI2_MOSI_PIN           PC3

// Dual IMU configuration
#define USE_DUAL_GYRO
#define USE_TARGET_IMU_HARDWARE_DESCRIPTORS

// Primary IMU: ICM-42688P on SPI1
#define USE_IMU_ICM42605        // ICM42605 driver supports ICM42688P (chip ID 0x47)
#define IMU_ICM42605_ALIGN      8   // CW270_DEG_FLIP (PITCH_180_YAW_90 equivalent)
#define ICM42605_CS_PIN         PC13
#define ICM42605_SPI_BUS        BUS_SPI1
#define ICM42605_EXTI_PIN       PE6

// Secondary IMU: BMI055/BMI088 on SPI1 (auto-detect by chip ID)
#define USE_IMU_BMI055
#define USE_IMU_BMI088
// Both BMI055 and BMI088 use same pins, detection via chip ID
#define BMI055_GYRO_CS_PIN      PC14
#define BMI055_GYRO_EXTI_PIN    PE5
#define BMI055_ACC_CS_PIN       PC15
#define BMI055_ACC_EXTI_PIN     PE4
#define IMU_BMI055_ALIGN        3   // CW180_DEG (PITCH_180 equivalent)

// FRAM storage (Ramtron/Cypress)
#define USE_FLASHFS
#define USE_FLASH_CHIP
#define USE_FLASH_RAMTRON
#define RAMTRON_CS_PIN          PD4
#define RAMTRON_SPI_BUS         BUS_SPI2
#define ENABLE_BLACKBOX_LOGGING_ON_SPIFLASH_BY_DEFAULT

// ADC - Battery and current monitoring
#define USE_ADC
#define ADC_INSTANCE                ADC1
#define ADC_CHANNEL_1_PIN           PC5     // VBAT1
#define ADC_CHANNEL_2_PIN           PC4     // CURR1
#define ADC_CHANNEL_3_PIN           PB1     // VBAT2
#define ADC_CHANNEL_4_PIN           PA2     // CURR2
#define ADC_CHANNEL_5_PIN           PA4     // 5V monitor

#define VBAT_ADC_CHANNEL            ADC_CHN_1
#define CURRENT_METER_ADC_CHANNEL   ADC_CHN_2

// Voltage and current scales from Pixhawk 6C hardware
#define VBAT_SCALE_DEFAULT          1818    // 18.18 voltage divider
#define CURRENT_METER_SCALE         3636    // 36.36 A/V scale

// SD card via SDMMC2 (4-bit mode)
#define USE_SDCARD
#define USE_SDCARD_SDIO
#define SDCARD_SDIO_DEVICE          SDIODEV_2
#define SDCARD_SDIO_4BIT
#define SDCARD_SDIO_CLK_PIN         PD6
#define SDCARD_SDIO_CMD_PIN         PD7
#define SDCARD_SDIO_DATA0_PIN       PB14
#define SDCARD_SDIO_DATA1_PIN       PB15
#define SDCARD_SDIO_DATA2_PIN       PB3
#define SDCARD_SDIO_DATA3_PIN       PB4

// USB support
#define USE_USB_DETECT
#define USB_DETECT_PIN              PA9

// Features
#define DEFAULT_FEATURES        (FEATURE_TX_PROF_SEL | \
                                 FEATURE_OSD | \
                                 FEATURE_TELEMETRY | \
                                 FEATURE_CURRENT_METER | \
                                 FEATURE_VBAT | \
                                 FEATURE_BLACKBOX)

// PWM outputs (8 channels)
#define MAX_PWM_OUTPUT_PORTS    8

// GPIO port masks
#define TARGET_IO_PORTA         0xffff
#define TARGET_IO_PORTB         0xffff
#define TARGET_IO_PORTC         0xffff
#define TARGET_IO_PORTD         0xffff
#define TARGET_IO_PORTE         0xffff
