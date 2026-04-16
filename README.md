# rDUINOScope 2.0 - STM32F401 Motor Control Firmware

This repository contains only the STM32F401 slave firmware. The complete telescope controller is dual-MCU: ESP32-S3 handles connectivity and UI, while STM32F401 runs a bare-metal motor loop.

## Architecture

The STM32F401 runs a time-critical control loop without interrupts or OS overhead. It communicates with the ESP32 master via Modbus RTU over UART at 57600 baud.

The separation is intentional: WiFi introduces unpredictable latency that breaks stepper motor timing. By keeping motor control isolated on a dedicated MCU, we achieve microsecond-level precision.

### Why not a single MCU?

- ESP32 WiFi stack requires context switches that jitter stepper pulses
- Modbus RTU provides a clean, documented boundary layer
- Each MCU specializes in what it does best

## Code Structure

### Configuration: lib/telescope/config.h

Compile-time flags and hardware mappings. Select your stepper driver (TMC2208, TMC2209, or TMC2130):

```cpp
#define GEAR_RATIO  72UL	// Mechanical ratio (es. 144:1)
#define USE_ENCODER	// AS5048B magnetic encoders (I2C1)
// #define USE_RS485	// RS485 transceiver su PA15
#define DEBUG_SERIAL	// debug su Serial (USB CDC disabilitata → noop)
```
Pin assignments and mechanical constants are here.


### Register Map: lib/telescope/registers.h
Modbus register layout shared between Motor Control MCU (STM32F401CD) and Main Interface MCU (ESP32-S3). Coordinates are stored as `arcsec * 100` split across two 16-bit registers:
```cpp 
//File: registers.h
namespace Reg {
    // --- STM32F401 Read-Only regs ---
    constexpr uint16_t TARGET_RA_HI   = 0;  // 40001 - RA target, word alta
    constexpr uint16_t TARGET_RA_LO   = 1;  // 40002 - RA target, word bassa
    constexpr uint16_t TARGET_DEC_HI  = 2;  // 40003 - DEC target, word alta
    constexpr uint16_t TARGET_DEC_LO  = 3;  // 40004 - DEC target, word bassa
    constexpr uint16_t COMMAND        = 4;  // 40005 - comando

	// --- STM32F401 Write-Only regs ---
    constexpr uint16_t STATUS         = 5;  // 40006 - stato telescopio
    constexpr uint16_t CURRENT_RA_HI  = 6;  // 40007
    constexpr uint16_t CURRENT_RA_LO  = 7;  // 40008
    constexpr uint16_t CURRENT_DEC_HI = 8;  // 40009
    constexpr uint16_t CURRENT_DEC_LO = 9;  // 40010
    constexpr uint16_t ERROR_CODE     = 10; // 40011
}
```
Example: 180.0 degrees RA = 180 * 3600 * 100 = 64,800,000 arcsec*100

Command values written to `COMMAND` (`40005`):

| Value | Name          | Behavior |
| :---- | :------------ | :------- |
| 0     | IDLE          | No new command |
| 1     | GOTO          | Move once to `TARGET_RA/DEC`, then start sidereal tracking |
| 2     | STOP          | Controlled decelerated stop |
| 3     | SYNC          | Set current position to `TARGET_RA/DEC`, then start tracking |
| 4     | FOLLOW_TARGET | Manual mode: continuously follow changes in `TARGET_RA/DEC`, preserving the current tracking state |

### Modbus Slave: `lib/Modbus/modbus_slave.h/cpp`
Implements Modbus RTU FC03 (read), FC06 (write single), FC16 (write multiple) with CRC16.

Non-blocking design: accumulates bytes in a buffer and processes complete frames when the serial line goes quiet for 2ms. Optional RS485 half-duplex support.

Why Modbus? It makes the device interface agnostic, so as long as you're able to write the registers you can use whatever you may like on the other side!

### Motor Driver: lib/TMC-Driver/tmc_driver.h/cpp
Abstracts TMC2208/2209/2130 stepper drivers. Compile-time template selects the driver type to avoid runtime overhead. Currently, supporting `TMC2208/TMC2209` and `TMC2130`.

### LED Status: lib/Utils/led_status.h
Non-blocking state machine blinks PC13 without any delay() calls:

| Status        | Pattern                  | Meaning                  |
| :------------ | :----------------------- | :----------------------- |
| IDLE          | Solid ON                 | Ready/no motion          |
| SLEWING       | 4 Hz blink               | GOTO in progress         |
| TRACKING      | 0.5 Hz blink             | Sidereal tracking active |
| ERROR         | 3 fast blinks + 1s pause | Position/driver fault    |

Each `update()` call takes microseconds — just checks elapsed time and toggles GPIO.
___
## Command Flow: How GOTO Works
1. **ESP32** writes RA/DEC target and COMMAND=1 to Modbus registers
2. **STM32** detects command change in main loop
3. **AccelStepper starts** both motors with trapezoidal velocity profile:
	- Accelerate at 500 steps/s² up to MAX_SPEED (2000 steps/s)
	- Coast at constant speed
	- Decelerate 500 steps/s² to arrive at target with zero velocity
4. **STATUS = SLEWING** while motors are moving
5. Motors arrive at target, distance-to-go becomes zero
6. **STM32** automatically transitions to sidereal tracking on RA only:
	```cpp
 	axisRA.setMaxSpeed(SIDEREAL_RATE_HZ * 1.1f);  // ~5.9 Hz
 	axisRA.moveTo(axisRA.currentPosition() + STEPS_PER_REV * 100);  // Very long move
	```
7. **STATUS = TRACKING** — RA axis continuously rotates to follow Earth rotation

### Manual Target Follow
Writing `COMMAND=4` enables manual/follow mode. In this mode the STM32 keeps polling
`TARGET_RA_*` and `TARGET_DEC_*`; whenever either target differs from the last accepted
target, it updates the AccelStepper destination with `moveTo()`.

Unlike `GOTO`, this mode does not decide the sidereal tracking state by itself. If
sidereal tracking was already active when `FOLLOW_TARGET` was selected, the firmware
resumes tracking after each target correction. If tracking was off, it remains off, so
the ESP32 can stream small RA/DEC updates from a joystick, buttons, or another
manual-control UI until it writes `STOP`, `GOTO`, `SYNC`, or `IDLE`.

### STOP Commands
The firmware separates controlled stops from hard emergency stops:

| Source        | Action                        | Motor enable state          | Position handling                   |
| :------------ | :---------------------------- | :-------------------------- | :---------------------------------- |
| Modbus STOP   | Decelerated stop with ramp    | RA/DEC remain energized     | Final reached RA/DEC is saved       |
| PA0 hard stop | Immediate stop, no decel ramp | RA/DEC drivers are disabled | Current RA/DEC is saved immediately |

#### Modbus Controlled STOP
Writing `COMMAND=2` to register `40005` requests a controlled stop. This is intended for the ESP32 or any Modbus master that wants to interrupt a GOTO or tracking without dropping motor holding torque.

Behavior:

1. `trackingActive` is cleared.
2. Any GOTO completion auto-tracking is cancelled.
3. RA and DEC remain enabled.
4. `AccelStepper::stop()` is applied to both axes, so the motors decelerate using the configured acceleration.
5. When both axes reach zero `distanceToGo()`, the firmware saves the reached RA/DEC into `CURRENT_RA_*` and `CURRENT_DEC_*`.
6. `STATUS` returns to `IDLE`.

This means the ESP32 can send STOP, wait until `STATUS=IDLE`, then read the final RA/DEC registers as the actual stopped position.

#### Hardwired Emergency STOP
The hardwired stop input is `PA0`, configured as `INPUT_PULLUP` and active LOW. Pulling PA0 to GND triggers an immediate stop path:

1. RA and DEC targets are forced to the current stepper positions.
2. Tracking, GOTO, and any controlled-stop state are cancelled.
3. `RA_EN` and `DEC_EN` are driven inactive, so the motors are no longer energized.
4. The current RA/DEC registers are updated immediately.

This path is intended for fault or emergency conditions, not for normal ESP32-requested pauses.

### SYNC Command
Tells STM32 "I'm already looking at these coordinates, don't move." Used after manual adjustment or encoder sync:
``` cpp
case Cmd::SYNC:
    axisRA.setCurrentPosition(arcsec100ToSteps(targetRA));
    axisDEC.setCurrentPosition(arcsec100ToSteps(targetDEC));
    startTracking();
    break;
```

___
## Encoder Feedback (Optional)
When `USE_ENCODER` is enabled, two AS5048B magnetic encoders on I2C1 provide absolute position feedback:

```C++
int32_t encoderToSteps(AMS_AS5048B& enc, float& lastAngle, int32_t& accumSteps) {
    float angle = enc.angleR(U_DEG, true);  // 14-bit reading with moving average
    float delta = angle - lastAngle;
    
    // Handle 0°/360° wrap-around
    if (delta > 180.0f) delta -= 360.0f;
    else if (delta < -180.0f) delta += 360.0f;
    
    accumSteps += (int32_t)(delta / 360.0f * STEPS_PER_REV);
    lastAngle = angle;
    return accumSteps;
}
```
Fault detection: If motors are idle but encoder position drifts >50 steps from step count, the system enters ERROR state and sets error code 0x01 (position fault). This catches mechanical slip or lost steps.

___
## Design Principles 
1. **No dynamic allocation** — Everything is stack or global. No malloc, no heap fragmentation.

2. **No blocking calls** — All I/O is polling-based. No Serial.println() in the motor loop; all debug output is deferred.

3. **Coordinate conversion at boundaries** — ESP32 sends arcsec*100; STM32 converts to steps internally. Keeps math deterministic.

4. **Compile-time selection** — Driver choice, encoder presence, RS485 support determined at build time. No runtime overhead for unused features.

5. **Fail-visible** — Errors set STATUS=ERROR and halt motors. Never silent failures or stuck states.

6. **Isolated timing loops** — AccelStepper, encoder polling, LED blinking, Modbus processing all run at their natural rate determined by the main loop frequency.
___
## Hardware Pin Assignments

### Motor Control (STM32F401 Port B)
| Function     | Pin  | Purpose                   |
| :----------- | :--- | :------------------------ |
| RA_STEP      | PB0  | Pulse to TMC Stepper      |
| RA_DIR	     | PB1  | Direction signal          |
| RA_ENABLE	   | PB2  | Active LOW (motor on/off) |
| DEC_STEP	   | PB8  | Pulse to TMC Stepper      |
| DEC_DIR	     | PB9  | Direction signal          |
| DEC_ENABLE   | PB12 | Active LOW                |
| FOCUS_STEP   | PB13 |	Pulse to TMC Stepper      |
| FOCUS_DIR    | PB14 |	Direction signal          |
| FOCUS_ENABLE | PB15 |	Active LOW                |

### Communication (STM32F401 Port A)
| Function     | Pin  | Notes                   |
| :----------- | :--- | :---------------------- |
| Modbus TX    | PA9  | UART1 → ESP32 RX        |
| Modbus RX    | PA10 | UART1 ← ESP32 TX        |
| Hard STOP    | PA0  | Active LOW, pull to GND |
| TMC TX       | PA11 | UART6 (half-duplex)     |
| TMC RX       | PA12 | UART6 (half-duplex)     |
| RS485 DE/RE  | PA15 | Optional, active HIGH   |

### Encoders (STM32F401 Port B)
| Function     | Pin  | Notes                  |
| :----------- | :--- | :--------------------- |
| Encoder SCL  | PB6  | I2C1 shared bus        |
| Encoder SDA  | PB7  | I2C1 shared bus        |

## Modbus Protocol Examples
### Read Current RA Position
Request (read registers 40007-40008):
``` Code
01 03 00 06 00 02 64 39
│  │  │  │  │  │  └─ CRC-16
│  │  │  │  │  └──── Count = 2 registers
│  │  │  │  └─────── Start address = 6 (40007)
│  │  │  └────────── Function = 03 (read holding)
│  │  └───────────── Reserved
│  └──────────────── Slave ID = 1
```
Response: `01 03 04 03D5 9000 xxxx` = RA coordinate 180.0 degrees

### Send GOTO Command
Write registers 40001-40005 with RA=180.0°, DEC=-5.0°, COMMAND=1:

``` Code
RA  = 180.0 * 3600 * 100 = 64800000   = 0x03D5_C500
DEC = -5.0  * 3600 * 100 = -1800000   = 0xFFE4_88C0
```
Frame: 01 10 0000 0005 0A 03D5 9000 FFE4 88C0 0001 xxxx
___
### Send Controlled STOP Command
Write `COMMAND=2` to register `40005`:

``` Code
01 06 00 04 00 02 xxxx
│  │  │  │  │  └──── Value = 2 (STOP)
│  │  │  │  └─────── Register low byte
│  │  │  └────────── Register = 4 (40005)
│  │  └───────────── Function = 06 (write single register)
│  └──────────────── Slave ID = 1
└─────────────────── CRC omitted here as xxxx
```

After this command, wait for `STATUS=IDLE`, then read `CURRENT_RA_*` and `CURRENT_DEC_*` to get the final stopped position.
___
## Future Enhancements
- [ ] Closed-loop position correction using encoder feedback
- [ ] Meridian flip auto-detection
- [ ] Polar alignment assistant
- [ ] Session logging to SD card
- [ ] Over-the-air firmware updates (ESP32)
