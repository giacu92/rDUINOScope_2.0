#pragma once
#include <stdint.h>

// Indici nei holding registers (0-based internamente, Modbus è 1-based)
namespace Reg {
    // --- Scritti da ESP32 (master) ---
    constexpr uint16_t TARGET_RA_HI   = 0;  // 40001 - RA target, word alta
    constexpr uint16_t TARGET_RA_LO   = 1;  // 40002 - RA target, word bassa
    constexpr uint16_t TARGET_DEC_HI  = 2;  // 40003 - DEC target, word alta
    constexpr uint16_t TARGET_DEC_LO  = 3;  // 40004 - DEC target, word bassa
    constexpr uint16_t COMMAND        = 4;  // 40005 - comando

    // --- Letti da ESP32 (master) ---
    constexpr uint16_t STATUS         = 5;  // 40006 - stato telescopio
    constexpr uint16_t CURRENT_RA_HI  = 6;  // 40007
    constexpr uint16_t CURRENT_RA_LO  = 7;  // 40008
    constexpr uint16_t CURRENT_DEC_HI = 8;  // 40009
    constexpr uint16_t CURRENT_DEC_LO = 9;  // 40010
    constexpr uint16_t ERROR_CODE     = 10; // 40011

    constexpr uint16_t TOTAL          = 11;
}

// Comandi (registro COMMAND)
namespace Cmd {
    constexpr uint16_t IDLE  = 0;
    constexpr uint16_t GOTO  = 1;
    constexpr uint16_t STOP  = 2;
    constexpr uint16_t SYNC  = 3;  // "qui punta già a queste coordinate"
}

// Stati (registro STATUS)
namespace Status {
    constexpr uint16_t IDLE     = 0;
    constexpr uint16_t SLEWING  = 1;
    constexpr uint16_t TRACKING = 2;
    constexpr uint16_t ERROR    = 3;
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
