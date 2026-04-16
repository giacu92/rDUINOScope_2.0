#pragma once
#include <stdint.h>

// Indici nei holding registers (0-based internamente, Modbus è 1-based)
namespace Reg {
    // --- Modbus Master request registers: written by ESP32, observed by STM32 ---
    constexpr uint16_t REQ_TARGET_RA_HI   = 0;  // 40001 - RA target, word alta
    constexpr uint16_t REQ_TARGET_RA_LO   = 1;  // 40002 - RA target, word bassa
    constexpr uint16_t REQ_TARGET_DEC_HI  = 2;  // 40003 - DEC target, word alta
    constexpr uint16_t REQ_TARGET_DEC_LO  = 3;  // 40004 - DEC target, word bassa
    constexpr uint16_t REQ_COMMAND        = 4;  // 40005 - comando richiesto da ESP32; STM32 non lo modifica

    // --- Modbus Slave response registers: written by STM32, read by ESP32 ---
    constexpr uint16_t RES_STATUS         = 5;  // 40006 - stato telescopio
    constexpr uint16_t RES_CURRENT_RA_HI  = 6;  // 40007
    constexpr uint16_t RES_CURRENT_RA_LO  = 7;  // 40008
    constexpr uint16_t RES_CURRENT_DEC_HI = 8;  // 40009
    constexpr uint16_t RES_CURRENT_DEC_LO = 9;  // 40010
    constexpr uint16_t RES_ERROR_CODE     = 10; // 40011

    // --- Milestone 0 request registers ---
    constexpr uint16_t REQ_TRACKING_ENABLE = 11; // 40012 - 0=off, 1=on
    constexpr uint16_t REQ_TRACKING_MODE   = 12; // 40013 - 0=lunar, 1=sidereal, 2=solar
    constexpr uint16_t REQ_MOTORS_ENABLE   = 13; // 40014 - 0=disabled, 1=enabled
    constexpr uint16_t REQ_JOG_AXIS        = 14; // 40015 - 0=RA, 1=DEC
    constexpr uint16_t REQ_JOG_DIRECTION   = 15; // 40016 - 0=negative/west/south, 1=positive/east/north
    constexpr uint16_t REQ_JOG_SPEED       = 16; // 40017 - profilo velocita definito da STM32

    // Handshake bit: ESP32 sets this to 1 after writing REQ_COMMAND; STM32
    // clears it to 0 after copying the command.
    constexpr uint16_t REQ_COMMAND_PENDING = 17; // 40018 - 1=comando pendente; STM32 azzera dopo consumo

    constexpr uint16_t TOTAL          = 18;
}

// Comandi (registro REQ_COMMAND)
namespace Cmd {
    constexpr uint16_t IDLE  = 0;
    constexpr uint16_t GOTO  = 1;
    constexpr uint16_t STOP  = 2;
    constexpr uint16_t SYNC  = 3;  // "qui punta già a queste coordinate"
    constexpr uint16_t FOLLOW_TARGET = 4;  // manual mode: follow REQ_TARGET_RA/DEC updates

    // Nuovi comandi Milestone 0. I valori 3 e 4 restano compatibili.
    constexpr uint16_t SET_TRACKING = 5;
    constexpr uint16_t SET_MOTORS   = 6;
    constexpr uint16_t JOG_START    = 7;
    constexpr uint16_t JOG_STOP     = 8;
}

// Stati (registro RES_STATUS)
namespace Status {
    constexpr uint16_t IDLE     = 0;
    constexpr uint16_t SLEWING  = 1;
    constexpr uint16_t TRACKING = 2;
    constexpr uint16_t ERROR    = 3;
    constexpr uint16_t MOTORS_DISABLED = 4;
    constexpr uint16_t MANUAL_JOG      = 5;
}

namespace TrackingMode {
    constexpr uint16_t LUNAR    = 0;
    constexpr uint16_t SIDEREAL = 1;
    constexpr uint16_t SOLAR    = 2;
}

namespace JogAxis {
    constexpr uint16_t RA_AXIS  = 0;
    constexpr uint16_t DEC_AXIS = 1;
}

namespace JogDirection {
    constexpr uint16_t NEGATIVE = 0;
    constexpr uint16_t POSITIVE = 1;
}

namespace JogSpeed {
    constexpr uint16_t GUIDE  = 1;
    constexpr uint16_t CENTER = 2;
    constexpr uint16_t SLEW   = 3;
}

// Helper per encode/decode coordinate 32bit su due registri 16bit.
// RA usa valori unsigned normalizzati, DEC usa valori signed.
// Unità interna: arcsec * 100 (0.01 arcsec di risoluzione)
inline void encode32(uint32_t val, uint16_t &hi, uint16_t &lo) {
    hi = (uint16_t)(val >> 16);
    lo = (uint16_t)(val & 0xFFFF);
}

inline uint32_t decode32(uint16_t hi, uint16_t lo) {
    return ((uint32_t)hi << 16) | lo;
}

inline void encode32Signed(int32_t val, uint16_t &hi, uint16_t &lo) {
    encode32((uint32_t)val, hi, lo);
}

inline int32_t decode32Signed(uint16_t hi, uint16_t lo) {
    return (int32_t)decode32(hi, lo);
}
