# IOMCU Integration Guide

This document describes how to integrate IOMCU support into INAV.

## Files Created

### IOMCU Firmware (9 files)
- `src/main/iomcu_firmware/iomcu_main.h/c` - Main firmware loop
- `src/main/iomcu_firmware/iomcu_registers.h/c` - Register system
- `src/main/iomcu_firmware/iomcu_protocol_handler.h/c` - Serial communication
- `src/main/iomcu_firmware/iomcu_safety.h/c` - Safety button logic
- `src/main/iomcu_firmware/iomcu_pwm.h/c` - PWM/DShot driver
- `src/main/iomcu_firmware/stm32f1xx_iomcu.ld` - Linker script
- `src/main/iomcu_firmware/startup_stm32f103.s` - Startup code

### FMU Driver (4 files)
- `src/main/drivers/iomcu/iomcu_protocol.h` - Shared protocol definitions
- `src/main/drivers/iomcu/iomcu.h/c` - Main FMU driver
- `src/main/drivers/iomcu/iomcu_uploader.h/c` - Firmware uploader
- `src/main/drivers/pwm_output_iomcu.c` - PWM routing layer

## Integration Points

### 1. PWM Output System (`src/main/drivers/pwm_output.c`)

Add after includes (line ~46):
```c
#ifdef USE_IOMCU
#include "iomcu/iomcu.h"

// External declarations
extern bool pwmIOMCUMotorEnabled(void);
extern void pwmIOMCUMotorEnable(void);
extern void pwmIOMCUWriteMotor(uint8_t index, uint16_t value);
extern uint8_t pwmIOMCUGetChannelCount(void);
extern void pwmIOMCUSetChannelCounts(uint8_t fmuCount, uint8_t iomcuCount);
#endif
```

In `pwmMotorPreconfigure()` function (around line 500):
```c
void pwmMotorPreconfigure(void)
{
    initMotorProtocol = motorConfig()->motorPwmProtocol;

#ifdef USE_IOMCU
    // Try to initialize IOMCU
    pwmIOMCUMotorEnable();

    if (pwmIOMCUMotorEnabled()) {
        // IOMCU available - adjust channel counts
        uint8_t fmuChannels = MAX_PWM_OUTPUT_PORTS;
        uint8_t iomcuChannels = pwmIOMCUGetChannelCount();
        pwmIOMCUSetChannelCounts(fmuChannels, iomcuChannels);

        // Create mixed write function
        motorWritePtr = pwmWriteMotorMixed;  // Define this function
        return;
    }
#endif

    // Existing FMU-only logic continues...
}
```

Add new mixed write function:
```c
#ifdef USE_IOMCU
static void pwmWriteMotorMixed(uint8_t index, uint16_t value)
{
    if (index < MAX_PWM_OUTPUT_PORTS) {
        // FMU motor - use standard write
        pwmWriteStandard(index, value);
    } else {
        // IOMCU motor
        pwmIOMCUWriteMotor(index, value);
    }
}
#endif
```

### 2. Scheduler (`src/main/fc/fc_tasks.c`)

Add task ID (around line 60):
```c
typedef enum {
    // ... existing tasks ...
#ifdef USE_IOMCU
    TASK_IOMCU,
#endif
    TASK_COUNT,
} cfTaskId_e;
```

Add task definition (around line 200):
```c
#ifdef USE_IOMCU
#include "drivers/iomcu/iomcu.h"
#endif

cfTask_t cfTasks[TASK_COUNT] = {
    // ... existing tasks ...
#ifdef USE_IOMCU
    [TASK_IOMCU] = {
        .taskName = "IOMCU",
        .taskFunc = iomcuTask,
        .desiredPeriod = TASK_PERIOD_HZ(50),  // 20ms = 50Hz
        .staticPriority = TASK_PRIORITY_MEDIUM,
    },
#endif
};
```

### 3. CLI Commands (`src/main/fc/cli.c`)

Add command handler (in CLI command table):
```c
#ifdef USE_IOMCU
static void cliIomcu(char *cmdline)
{
    if (isEmpty(cmdline)) {
        // Status
        cliPrintLinef("IOMCU: %s", iomcuIsHealthy() ? "OK" : "FAIL");
        cliPrintLinef("Safety: %s", iomcuIsSafetyOff() ? "OFF" : "ON");
        cliPrintLinef("Channels: %d", iomcuGetChannelCount());
    }
    else if (strcasecmp(cmdline, "update") == 0) {
        cliPrintLine("Forcing firmware update...");
        if (iomcuForceUpdate()) {
            cliPrintLine("Update successful");
        } else {
            cliPrintLine("Update failed");
        }
    }
    else if (strcasecmp(cmdline, "safety_off") == 0) {
        iomcuForceSafetyOff();
        cliPrintLine("Safety forced off");
    }
    else {
        cliPrintLine("Usage: iomcu [status|update|safety_off]");
    }
}

// Add to command table
const clicmd_t cmdTable[] = {
    // ... existing commands ...
#ifdef USE_IOMCU
    CLI_COMMAND_DEF("iomcu", "IOMCU status and control", NULL, cliIomcu),
#endif
};
#endif
```

### 4. OSD Warning (`src/main/io/osd.c`)

Add after other warnings (around line 2000):
```c
#ifdef USE_IOMCU
#include "drivers/iomcu/iomcu.h"

// In the warning display function
if (iomcuIsInitialized() && !iomcuIsHealthy()) {
    osdDisplayWrite(x, y, ATTR_WARNING, "IOMCU LOST");
    y++;
}
#endif
```

### 5. Target Configuration

#### `src/main/target/PIXHAWK6C/target.h`

Add (already exists, just confirm):
```c
#define USE_UART6              // IOMCU
#define UART6_TX_PIN PC6
#define UART6_RX_PIN PC7

#define USE_IOMCU              // Enable IOMCU support
```

#### `src/main/target/PIXHAWK6C_MINI/target.h`

Add:
```c
#define USE_UART6              // IOMCU
#define UART6_TX_PIN PC6
#define UART6_RX_PIN PC7

#define USE_IOMCU              // Enable IOMCU support
```

### 6. Build System

See separate build integration files:
- `src/utils/bin2c.py`
- `cmake/iomcu-firmware.cmake`
- `src/main/iomcu_firmware/CMakeLists.txt`
- Update `src/main/target/PIXHAWK6C/CMakeLists.txt`

## Testing

1. **Build Test**: `cmake --build build --target PIXHAWK6C`
2. **Flash IOMCU** via SWD for initial test
3. **Verify Communication**: Check CLI `iomcu` command
4. **Test PWM Outputs**: Connect 16 motors/servos
5. **Test Safety Button**: Press for 1 second, verify toggle
6. **Test Auto-Update**: Power cycle with CRC mismatch
7. **Test Failsafe**: Disconnect FMU, verify IOMCU holds values

## Optional Features (Future)

- RC input via IOMCU (not implemented yet)
- Failsafe mixer for fixed-wing (not implemented yet)
- Voltage monitoring via IOMCU ADC (not implemented yet)
