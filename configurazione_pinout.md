# Configurazione Pinout Progetto

## Tabella Principale: Funzioni Motori e Sensori

| Function | Pin | Notes |
| :--- | :--- | :--- |
| **RA STEP** | PB0 | Step pulse to TMC2208 |
| **RA DIR** | PB1 | Direction |
| **RA ENABLE** | PB2 | Active LOW (TMC2208) |
| **DEC STEP** | PB8 | Step pulse to TMC2208 |
| **DEC DIR** | PB9 | Direction |
| **DEC ENABLE** | PB12 | Active LOW |
| **FOCUS STEP** | PB13 | Step pulse to TMC2208 |
| **FOCUS DIR** | PB14 | Direction |
| **FOCUS ENABLE** | PB15 | Active LOW |
| **Modbus TX** | PA9 | Serial1 -> ESP32 RX |
| **Modbus RX** | PA10 | Serial1 -> ESP32 TX |
| **RS485 DE/RE** | PA15 | Optional — #define USE_RS485 |
| **Encoder RA SDA** | PB7 | I2C1 — AS5048B addr 0x40 (SDA1) |
| **Encoder RA SCL** | PB6 | I2C1 (SCL1) |
| **Encoder DEC SDA** | PB7 | I2C1 — AS5048B addr 0x41 (SDA1) |
| **Encoder DEC SCL** | PB6 | I2C1 (shared bus) (SCL1) |
| **Status LED** | PC13 | Active LOW — built-in Black Pill |

## Configurazione Stepper (SPI / CS)

| Function | Pin | Notes |
| :--- | :--- | :--- |
| **TMC_SDO** | PA6 | Stepper SDO/CFG0 (MISO1) |
| **TMC_SDI** | PA7 | Stepper SDI/CFG1 (MOSI1) |
| **TMC_SCK** | PA5 | Stepper SCK/CFG2 (SCK1) |
| **TMC_CS_RA** | PA1 | Stepper CS_RA/CFG3_RA |
| **TMC_CS_DEC** | PB5 | Stepper CS_RA/CFG3_RA |
| **TMC_CS_FOCUS** | PB10 | Stepper CS_RA/CFG3_RA |

## Riepilogo Mappatura Pin (GPIO Port)

### Porta A (PA)
| Pin | Label / Note |
| :--- | :--- |
| PA0 | WKUP |
| PA1 | |
| PA2 | TX2 |
| PA3 | RX2 |
| PA4 | FLASH_CS |
| PA5 | SCK1 |
| PA6 | MISO1 |
| PA7 | MOSI1 |
| PA8 | SCL3 |
| PA9 | |
| PA10 | |
| PA11 | TX6 - USB |
| PA12 | RX6 - USB |
| PA13 | SWDIO |
| PA14 | SWCLK |
| PA15 | |

### Porta B (PB)
| Pin | Label / Note |
| :--- | :--- |
| PB0 | |
| PB1 | |
| PB2 | BOOT1 |
| PB3 | SDA2 |
| PB4 | SDA3 |
| PB5 | |
| PB6 | SCL1 |
| PB7 | SDA1 |
| PB8 | SCL1* |
| PB9 | SDA1* |
| PB10 | SCL2 |
| PB11 | NC |
| PB12 | |
| PB13 | |
| PB14 | |
| PB15 | |

### Porta C (PC)
| Pin | Label / Note |
| :--- | :--- |
| PC0 - PC12 | NC (Not Connected) |
| PC13 | LED (Built-in) |
| PC14 | only input |
| PC15 | only input |

## Note Finali
* **BOOT1***: Can be only output.
* **WKUP**: Posso svegliare il chip.
