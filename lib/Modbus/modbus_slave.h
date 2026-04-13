#pragma once
#include <Arduino.h>
#include "registers.h"

class ModbusSlave {
public:
    // registers: array condiviso con la logica applicativa
    // deviceId:  indirizzo slave Modbus (1-247)
    // serial:    porta HardwareSerial da usare (Serial1, Serial2...)
    // dePin:     pin DE/RE per RS485 (-1 se non usato, es. UART diretta)
    ModbusSlave(uint16_t* registers, uint8_t numRegs,
                uint8_t deviceId, HardwareSerial& serial, int8_t dePin = -1);

    void begin(uint32_t baud);

    // Da chiamare nel loop() — processa eventuali frame ricevuti
    void update();

private:
    uint16_t*       _regs;
    uint8_t         _numRegs;
    uint8_t         _deviceId;
    HardwareSerial& _serial;
    int8_t          _dePin;

    // Buffer ricezione
    static constexpr uint8_t BUF_SIZE = 64;
    uint8_t  _buf[BUF_SIZE];
    uint8_t  _len = 0;
    uint32_t _lastByteTime = 0;

    // Timeout inter-frame RTU: 3.5 caratteri
    // A 9600 baud → ~4ms, a 115200 → ~0.3ms, usiamo minimo 2ms
    static constexpr uint32_t FRAME_TIMEOUT_US = 2000;

    void     processFrame();
    void     handleReadHoldingRegs(uint16_t startReg, uint16_t count);
    void     handleWriteMultipleRegs(uint16_t startReg, uint16_t count, uint8_t* data);
    void     handleWriteSingleReg(uint16_t reg, uint16_t value);
    void     sendResponse(uint8_t* data, uint8_t len);
    void     sendException(uint8_t fnCode, uint8_t exCode);
    uint16_t crc16(uint8_t* buf, uint8_t len);
    void     setTx(bool transmit);
};