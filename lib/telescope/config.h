#pragma once

// ═══════════════════════════════════════════════════════
//  Configurazione hardware — commenta/decommenta a piacere
// ═══════════════════════════════════════════════════════

// Abilita encoder magnetici AS5048B (I2C)
// Se disabilitato, la posizione reale = posizione step
#define USE_ENCODER

// Abilita RS485 (pin DE/RE per transceiver)
// Se disabilitato, Modbus gira su UART diretta (es. USB-TTL per test)
// #define USE_RS485

// Abilita output debug su Serial (USB)
#define DEBUG_SERIAL

// ═══════════════════════════════════════════════════════
//  Parametri meccanici
// ═══════════════════════════════════════════════════════
constexpr uint32_t STEPS_PER_REV    = 3200 * 144;
constexpr float    MAX_SPEED        = 2000.0f;
constexpr float    ACCELERATION     = 500.0f;
constexpr float    SIDEREAL_RATE_HZ = (float)STEPS_PER_REV / 86164.0f;

// Soglia errore encoder oltre la quale si segnala fault (in step)
constexpr int32_t  ENCODER_ERROR_THRESHOLD = 50;