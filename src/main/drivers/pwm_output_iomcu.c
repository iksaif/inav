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

#include "pwm_output.h"
#include "iomcu/iomcu.h"

#include "common/log.h"

// IOMCU PWM output backend
// Provides transparent integration with INAV's PWM output system

static bool iomcuPwmEnabled = false;
static uint8_t fmuChannelCount = 0;
static uint8_t totalChannelCount = 0;

bool pwmIOMCUMotorEnabled(void)
{
    return iomcuPwmEnabled;
}

void pwmIOMCUMotorEnable(void)
{
    if (iomcuInit()) {
        iomcuPwmEnabled = true;
        LOG_INFO(IOMCU, "IOMCU PWM enabled, 8 additional channels available");
    } else {
        iomcuPwmEnabled = false;
        LOG_WARNING(IOMCU, "IOMCU init failed, using FMU-only mode");
    }
}

void pwmIOMCUWriteMotor(uint8_t index, uint16_t value)
{
    if (!iomcuPwmEnabled) {
        return;
    }

    // Check if this is an IOMCU channel
    if (index >= fmuChannelCount && index < totalChannelCount) {
        uint8_t iomcuChannel = index - fmuChannelCount;
        iomcuWritePwm(iomcuChannel, value);
    }
}

uint8_t pwmIOMCUGetChannelCount(void)
{
    return iomcuPwmEnabled ? iomcuGetChannelCount() : 0;
}

void pwmIOMCUSetChannelCounts(uint8_t fmuCount, uint8_t iomcuCount)
{
    fmuChannelCount = fmuCount;
    totalChannelCount = fmuCount + iomcuCount;
}

#endif // USE_IOMCU
