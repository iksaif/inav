# Pixhawk 6C / Pixhawk 6C Mini

Professional-grade flight controllers based on the Pixhawk FMUv6C Open Standard from Holybro. These controllers feature high-performance STM32H7 processors, vibration-isolated dual IMUs, and comprehensive sensor redundancy for demanding applications.

## Variants

Two variants share the same target with conditional compilation:

- **PIXHAWK6C** (Standard): 16 PWM outputs (8 FMU + 8 IOMCU), 9 serial ports
- **PIXHAWK6C_MINI** (Mini): 14 PWM outputs (6 FMU + 8 IOMCU), 8 serial ports (no TELEM3/UART2, no FMU PWM7-8)

Both variants support the same core functionality with auto-detection of hardware revisions.

## Hardware Info

### Core Processor
- **MCU**: STM32H743 (Cortex-M7 @ 480MHz, 2MB Flash, 1MB RAM)
- **Crystal**: 16 MHz HSE
- **IO Coprocessor**: STM32F103 (IOMCU, provides 8 additional PWM outputs with DShot support and hardware safety switch)

### Sensors

#### IMUs (Dual redundancy with auto-detection)
- **Primary**: ICM-42688P (SPI1, CS: PC13)
- **Secondary**: BMI055 (Rev 0/1 hardware, pre-Dec 2023) or BMI088 (Rev 2 hardware, Dec 2023+)
  - Both use same pins: Gyro CS PC14, Accel CS PC15
  - Auto-detected at boot via chip ID

#### Other Sensors
- **Barometer**: MS5611 (2×, I2C4 0x76/0x77), BMP388/DPS310/SPL06 supported
- **Magnetometer**: IST8310 (I2C4 internal 0x0C, I2C1 external)

### Storage
- **FRAM**: 32KB SPI (SPI2, CS: PD4) - Parameter storage
- **SD Card**: microSD via SDMMC2 (4-bit mode) - Blackbox logging

### Communication

#### Serial Ports (Standard variant)
- **VCP**: USB Virtual COM
- **UART1** (GPS1): TX PB6, RX PA10
- **UART2** (TELEM3): TX PD5, RX PA3 - *Not on Mini*
- **UART3** (Debug): TX PD8, RX PD9
- **UART5** (TELEM2): TX PC12, RX PD2, RTS PC8, CTS PC9
- **UART6** (IOMCU): TX PC6, RX PC7 - Internal use
- **UART7** (TELEM1): TX PE8, RX PE7, RTS PE9, CTS PE10
- **UART8** (GPS2): TX PE1, RX PE0

#### I2C Buses
- **I2C1**: External (GPS/compass) - SCL PB8, SDA PB7
- **I2C2**: External (expansion) - SCL PB10, SDA PB11
- **I2C4**: Internal (sensors) - SCL PD12, SDA PD13

#### SPI Buses
- **SPI1**: IMU sensors - SCK PA5, MISO PA6, MOSI PA7
- **SPI2**: FRAM storage - SCK PD3, MISO PC2, MOSI PC3

#### CAN Buses (pins defined, protocol not implemented)
- **CAN1**: TX PD1, RX PD0
- **CAN2**: TX PB13, RX PB5

### PWM Outputs

#### FMU Outputs (STM32H743)

**Standard (8 outputs)**
- **PWM 1-4**: TIM1 CH1-4 (PA8, PE11, PE13, PE14)
- **PWM 5-6**: TIM4 CH3-4 (PD14, PD15)
- **PWM 7-8**: TIM5 CH1-2 (PA0, PA1)

**Mini (6 outputs)**
- **PWM 1-4**: TIM1 CH1-4 (PA8, PE11, PE13, PE14)
- **PWM 5-6**: TIM4 CH3-4 (PD14, PD15)

#### IOMCU Outputs (STM32F103) - Both Variants

- **PWM 9-16**: 8 additional outputs via IOMCU coprocessor
- Supports standard PWM, Oneshot125, DShot150/300/600
- Independent failsafe operation with 1-second watchdog
- Hardware safety switch support
- Automatic firmware update at boot (CRC-based)

**Protocols**: PWM, Oneshot, DShot (all outputs)

**Total Outputs**: 16 (Standard) / 14 (Mini)

### Power Monitoring

#### ADC Channels
- **Battery 1**: Voltage PC5, Current PC4
- **Battery 2**: Voltage PB1, Current PA2
- **5V Rail**: PA4
- **HW Version**: PC0 (hardware variant detection)
- **HW Revision**: PC1 (hardware variant detection)

**Voltage/Current Scales**:
- `VBAT_SCALE_DEFAULT = 1818` (18.18 divider)
- `CURRENT_METER_SCALE = 3636` (36.36 A/V)

#### Power Control (GPIO defined, logic not implemented)
- VDD_3V3_SENSORS_EN: PB2
- nVDD_5V_PERIPH_EN: PE2
- nVDD_5V_HIPOWER_EN: PC10
- VDD_5V_PERIPH_nOC: PE3 (overcurrent detect)
- VDD_5V_HIPOWER_nOC: PC11 (overcurrent detect)
- VDD_BRICK_nVALID: PA15 (power brick detect)
- VDD_BRICK2_nVALID: PB12 (power brick detect)

### Other Features
- **LEDs**: Red PD10, Blue PD11 (active-low)
- **Buzzer**: PB0 (TIM3 CH3, PWM-driven)
- **USB**: OTG FS (PA11/PA12), VBUS detect PA9

## Documentation

### Official Hardware Documentation
- [Holybro Pixhawk 6C Docs](https://docs.holybro.com/autopilot/pixhawk-6c)
- [Holybro Pixhawk 6C Mini Docs](https://docs.holybro.com/autopilot/pixhawk-6c-mini)
- [Technical Specifications (Standard)](https://docs.holybro.com/autopilot/pixhawk-6c/technical-specification)
- [Technical Specifications (Mini)](https://docs.holybro.com/autopilot/pixhawk-6c-mini/technical-specification)

### Datasheets
- **MCU**: [STM32H743 Datasheet](https://www.st.com/resource/en/datasheet/stm32h743vi.pdf)
- **IMU Primary**: [ICM-42688-P Datasheet](https://invensense.tdk.com/download-pdf/icm-42688-p-datasheet/)
- **IMU Secondary (Rev 0/1)**: [BMI055 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi055-ds000.pdf)
- **IMU Secondary (Rev 2)**: [BMI088 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi088-ds001.pdf)
- **Barometer**: [MS5611 Datasheet](https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS5611-01BA03%7FB3%7Fpdf%7FEnglish%7FENG_DS_MS5611-01BA03_B3.pdf)
- **Magnetometer**: [IST8310 Datasheet](https://www.isentek.com/userfiles/files/IST8310%20Datasheet%20v1_8.pdf)

### Purchase
- [Holybro Store - Pixhawk 6C](https://holybro.com/products/pixhawk-6c)
- [Holybro Store - Pixhawk 6C Mini](https://holybro.com/products/pixhawk-6c-mini)

## Physical Specifications

### Pixhawk 6C Standard
- **Size**: 84.8 × 44 × 12.4 mm
- **Weight**: 59.3g (aluminum case), 34.6g (plastic case)
- **Operating Temperature**: -40°C to 85°C

### Pixhawk 6C Mini
- **Size**: 54.3 × 39 × 17.5 mm (Model A), 58.3 × 39 × 18.15 mm (Model B)
- **Weight**: 42.4g (Model A), 46.8g (Model B)
- **Operating Temperature**: -40°C to 85°C

## Build Targets

```bash
make PIXHAWK6C        # Standard variant (8 PWM, 9 UARTs)
make PIXHAWK6C_MINI   # Mini variant (6 PWM, 8 UARTs)
```

## TODO - Missing Features

The following features are present in hardware but not yet implemented in INAV:

### Medium Priority
- [ ] Add PID-based IMU heater control for thermal stability
- [ ] Implement compass magnetic field compensation for IMU heater interference
- [ ] Add runtime hardware version detection using ADC channels

### Low Priority
- [ ] Implement CAN bus driver and DroneCAN protocol support
- [ ] Implement power rail control and fault detection logic

## Notes

- **Hardware Auto-Detection**: BMI055 (Rev 0/1) vs BMI088 (Rev 2) is automatically detected at boot. No user configuration required.
- **IOMCU Support**: Full IOMCU coprocessor support with automatic firmware updates, 8 additional PWM outputs, DShot support, and hardware safety switch functionality.
- **Failsafe Operation**: IOMCU provides independent failsafe with 1-second FMU watchdog. If FMU communication is lost, IOMCU maintains last valid PWM values.
- **CAN Bus**: Pins are defined but protocol stack is not implemented. Cannot use CAN peripherals.
- **Build Size**: ~656KB firmware (36% of 2MB flash).

## Implementation Status

Core implementation complete and ready for hardware testing:
- ✅ All critical drivers implemented (FRAM, ICM-42688P, BMI055, BMI088)
- ✅ Dual IMU with auto-detection
- ✅ All UARTs, I2C, SPI buses configured
- ✅ FMU PWM outputs and timer hardware
- ✅ IOMCU coprocessor with 8 additional PWM outputs
- ✅ IOMCU DShot150/300/600 support
- ✅ IOMCU automatic firmware updates
- ✅ Hardware safety switch via IOMCU
- ✅ SD card blackbox logging
- ✅ Dual battery monitoring
- ✅ Both Standard and Mini variants

See project documentation for detailed feature roadmap.

### IOMCU Features

The IOMCU implementation provides professional-grade I/O expansion:
- **8 Additional Outputs**: PWM channels 9-16 via STM32F103 coprocessor
- **DShot Support**: DShot150/300/600 protocols with DMA-based generation
- **Safety Switch**: Hardware safety button with 1-second press debouncing
- **Failsafe**: Independent operation with 1-second FMU watchdog
- **Auto-Update**: Automatic firmware upload at boot with CRC verification
- **CLI Commands**: `iomcu` command for status, firmware update, and safety control
- **OSD Integration**: "IOMCU LOST" warning if communication fails

Use `iomcu` CLI command to check status or force firmware updates.

---

**Note**: Hardware not yet tested on real hardware. Please report any issues on the INAV GitHub repository.
