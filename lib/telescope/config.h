// ── config.h aggiornato ───────────────────────────────────────────────────
#pragma once

// ═══════════════════════════════════════════════════════════════════════════
//  rDuinoScope 2.0 — STM32F401 Configuration
// ═══════════════════════════════════════════════════════════════════════════

// ── Driver selection — scegli UNO ─────────────────────────────────────────
// #define TMC_DRIVER_2208   // TMC2208 STEP/DIR + UART (silent, no stallGuard)
#define TMC_DRIVER_2209      // TMC2209 STEP/DIR + UART (silent + stallGuard)
// #define TMC_DRIVER_2130   // TMC2130 STEP/DIR + SPI  (full featured)

// ── Feature flags ─────────────────────────────────────────────────────────
#define GEAR_RATIO  144UL	// Mechanical ratio (es. 144:1)
#define USE_ENCODER			// AS5048B magnetic encoders (I2C1)
// #define USE_RS485		// RS485 transceiver su PA15
#define DEBUG_SERIAL		// debug su Serial (USB CDC disabilitata → noop)

// ── Pin motori — tutti su PB ──────────────────────────────────────────────
constexpr uint8_t RA_STEP      = PB0;
constexpr uint8_t RA_DIR       = PB1;
constexpr uint8_t RA_EN        = PB2;

constexpr uint8_t DEC_STEP     = PB8;
constexpr uint8_t DEC_DIR      = PB9;
constexpr uint8_t DEC_EN       = PB12;

constexpr uint8_t FOCUS_STEP   = PB13;
constexpr uint8_t FOCUS_DIR    = PB14;
constexpr uint8_t FOCUS_EN     = PB15;

// ── Modbus — Serial1 (USART1) ─────────────────────────────────────────────
// PA9=TX, PA10=RX  (hardware USART1)
#define MODBUS_BAUDRATE 115200
constexpr uint8_t MODBUS_RX_PIN = PA10;
constexpr uint8_t MODBUS_TX_PIN = PA9;
constexpr uint8_t RS485_DE_PIN  = PA15;  // #ifdef USE_RS485

// ── TMC UART — Serial6 (USART6) ──────────────────────────────────────────
// PA11=TX, PA12=RX  (hardware USART6, USB OTG non usata)
// Single-wire half-duplex: collega PA11 e PA12 insieme → pin PDN_UART del TMC
// Nota: TMC2208/2209 usano un solo filo bidirezionale
#define TMC_SERIAL_BAUDRATE 115200
constexpr uint8_t TMC_SERIAL_RX    = PA12;
constexpr uint8_t TMC_SERIAL_TX    = PA11;

// ── TMC SPI — SPI1 (solo TMC2130) ────────────────────────────────────────
// PA5=SCK, PA6=MISO, PA7=MOSI
constexpr uint8_t TMC_SPI_SCK   = PA5;
constexpr uint8_t TMC_SPI_MISO  = PA6;
constexpr uint8_t TMC_SPI_MOSI  = PA7;
constexpr uint8_t TMC_CS_RA     = PA1;
constexpr uint8_t TMC_CS_DEC    = PB5;
constexpr uint8_t TMC_CS_FOCUS  = PB10;

// ── Encoder I2C1 — PB6=SCL, PB7=SDA ──────────────────────────────────────
constexpr uint8_t ENCODER_RA_ADDR  = 0x40;
constexpr uint8_t ENCODER_DEC_ADDR = 0x41;

// ── Parametri TMC (comuni a tutti i driver) ───────────────────────────────
constexpr uint16_t TMC_CURRENT_RA    = 800;   // mA RMS
constexpr uint16_t TMC_CURRENT_DEC   = 800;
constexpr uint16_t TMC_CURRENT_FOCUS = 400;
constexpr uint8_t  TMC_MICROSTEPS    = 16;

// ── Parametri meccanici ───────────────────────────────────────────────────
constexpr uint32_t STEPS_PER_REV         = 200UL * TMC_MICROSTEPS * GEAR_RATIO;
constexpr float    MAX_SPEED             = 2000.0f;
constexpr float    ACCELERATION          = 500.0f;
constexpr float    SIDEREAL_RATE_HZ      = (float)STEPS_PER_REV / 86164.0f;
constexpr int32_t  ENCODER_ERROR_THRESHOLD = 50;