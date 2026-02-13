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

#include "platform.h"
#include "build/debug.h"

#include "common/axis.h"
#include "common/maths.h"
#include "common/utils.h"

#include "drivers/system.h"
#include "drivers/time.h"
#include "drivers/io.h"
#include "drivers/bus.h"

#include "drivers/sensor.h"
#include "drivers/accgyro/accgyro.h"
#include "drivers/accgyro/accgyro_bmi055.h"


#if defined(USE_IMU_BMI055)

/*
  device registers, names follow datasheet conventions, with REGA_
  prefix for accel, and REGG_ prefix for gyro
 */
#define REGA_CHIPID        0x00
#define REGA_ERR_REG       0x02
#define REGA_STATUS        0x03
#define REGA_X_LSB         0x12
#define REGA_INT_STATUS_1  0x1D
#define REGA_TEMP_LSB      0x22
#define REGA_TEMP_MSB      0x23
#define REGA_CONF          0x40
#define REGA_RANGE         0x41
#define REGA_PWR_CONF      0x7C
#define REGA_PWR_CTRL      0x7D
#define REGA_SOFTRESET     0x7E
#define REGA_FIFO_CONFIG0  0x48
#define REGA_FIFO_CONFIG1  0x49
#define REGA_FIFO_DOWNS    0x45
#define REGA_FIFO_DATA     0x26
#define REGA_FIFO_LEN0     0x24
#define REGA_FIFO_LEN1     0x25

#define REGG_CHIPID        0x00
#define REGG_RATE_X_LSB    0x02
#define REGG_INT_CTRL      0x15
#define REGG_INT_STATUS_1  0x0A
#define REGG_INT_STATUS_2  0x0B
#define REGG_INT_STATUS_3  0x0C
#define REGG_FIFO_STATUS   0x0E
#define REGG_RANGE         0x0F
#define REGG_BW            0x10
#define REGG_LPM1          0x11
#define REGG_RATE_HBW      0x13
#define REGG_BGW_SOFTRESET 0x14
#define REGG_FIFO_CONFIG_1 0x3E
#define REGG_FIFO_DATA     0x3F


static void bmi055GyroInit(gyroDev_t *gyro)
{
    busSetSpeed(gyro->busDev, BUS_SPEED_INITIALIZATION);

    // Soft reset
    busWrite(gyro->busDev, REGG_BGW_SOFTRESET, 0xB6);
    delay(100);

    // ODR 2kHz, BW 532Hz
    busWrite(gyro->busDev, REGG_BW, 0x81);
    delay(1);

    // Enable sampling
    busWrite(gyro->busDev, REGG_INT_CTRL, 0x80);
    delay(1);

    busSetSpeed(gyro->busDev, BUS_SPEED_FAST);
}

static void bmi055AccInit(accDev_t *acc)
{
    busSetSpeed(acc->busDev, BUS_SPEED_INITIALIZATION);

    // Soft reset
    busWrite(acc->busDev, REGA_SOFTRESET, 0xB6);
    delay(100);

    // Active mode
    busWrite(acc->busDev, REGA_PWR_CONF, 0);
    delay(100);

    // ACC ON
    busWrite(acc->busDev, REGA_PWR_CTRL, 0x04);
    delay(100);

    // OSR4, ODR 1600Hz
    busWrite(acc->busDev, REGA_CONF, 0x8C);
    delay(1);

    // Range 12g
    busWrite(acc->busDev, REGA_RANGE, 0x02);
    delay(1);

    busSetSpeed(acc->busDev, BUS_SPEED_STANDARD);

    acc->acc_1G = 2048;
}

static bool bmi055GyroRead(gyroDev_t *gyro)
{
    uint8_t gyroRaw[6];

    if (busReadBuf(gyro->busDev, REGG_RATE_X_LSB, gyroRaw, 6)) {
        gyro->gyroADCRaw[X] = (float) int16_val_little_endian(gyroRaw, 0);
        gyro->gyroADCRaw[Y] = (float) int16_val_little_endian(gyroRaw, 1);
        gyro->gyroADCRaw[Z] = (float) int16_val_little_endian(gyroRaw, 2);
        return true;
    }

    return false;
}

static bool bmi055AccRead(accDev_t *acc)
{
    uint8_t buffer[7];

    if (busReadBuf(acc->busDev, REGA_STATUS, buffer, 2) && (buffer[1] & 0x80) && busReadBuf(acc->busDev, REGA_X_LSB, buffer, 7)) {
        // first byte is discarded, see datasheet
        acc->ADCRaw[X] = (float)(((int16_t)(buffer[2] << 8) | buffer[1]) * 3 / 4);
        acc->ADCRaw[Y] = (float)(((int16_t)(buffer[4] << 8) | buffer[3]) * 3 / 4);
        acc->ADCRaw[Z] = (float)(((int16_t)(buffer[6] << 8) | buffer[5]) * 3 / 4);
        return true;
    }

    return false;
}

static bool gyroDeviceDetect(busDevice_t * busDev)
{
    uint8_t attempts;

    busSetSpeed(busDev, BUS_SPEED_INITIALIZATION);

    for (attempts = 0; attempts < 5; attempts++) {
        uint8_t chipId;

        delay(100);
        busRead(busDev, REGG_CHIPID, &chipId);

        if (chipId == 0x0F) {
            return true;
        }
    }

    return false;
}

static bool accDeviceDetect(busDevice_t * busDev)
{
    uint8_t attempts;

    busSetSpeed(busDev, BUS_SPEED_INITIALIZATION);

    for (attempts = 0; attempts < 5; attempts++) {
        uint8_t chipId;

        delay(100);
        busRead(busDev, REGA_CHIPID, &chipId);

        // BMI055 accel chip ID is 0xFA (vs 0x1E for BMI088)
        if (chipId == 0xFA) {
            return true;
        }
    }

    return false;
}

bool bmi055AccDetect(accDev_t *acc)
{
    acc->busDev = busDeviceInit(BUSTYPE_ANY, DEVHW_BMI055_ACC, acc->imuSensorToUse, OWNER_MPU);
    if (acc->busDev == NULL) {
        return false;
    }

    if (!accDeviceDetect(acc->busDev)) {
        busDeviceDeInit(acc->busDev);
        return false;
    }

    acc->initFn = bmi055AccInit;
    acc->readFn = bmi055AccRead;
    acc->accAlign = acc->busDev->param;

    return true;
}

bool bmi055GyroDetect(gyroDev_t *gyro)
{
    gyro->busDev = busDeviceInit(BUSTYPE_ANY, DEVHW_BMI055_GYRO, gyro->imuSensorToUse, OWNER_MPU);
    if (gyro->busDev == NULL) {
        return false;
    }

    if (!gyroDeviceDetect(gyro->busDev)) {
        busDeviceDeInit(gyro->busDev);
        return false;
    }

    gyro->initFn = bmi055GyroInit;
    gyro->readFn = bmi055GyroRead;
    gyro->scale = 1.0f / 16.4f; // 16.4 dps/lsb
    gyro->intStatusFn = gyroCheckDataReady;
    gyro->gyroAlign = gyro->busDev->param;

    return true;
}

#endif /* USE_IMU_BMI055 */
