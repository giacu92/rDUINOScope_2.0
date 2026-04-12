#include "modbus_slave.h"

ModbusSlave::ModbusSlave(uint16_t* registers, uint8_t numRegs,
                         uint8_t deviceId, HardwareSerial& serial, int8_t dePin)
    : _regs(registers), _numRegs(numRegs),
      _deviceId(deviceId), _serial(serial), _dePin(dePin) {}

void ModbusSlave::begin(uint32_t baud) {
    _serial.begin(baud, SERIAL_8N1);
    delay(100);
    // Test byte per byte
    _serial.write((uint8_t)0xAA);
    _serial.write((uint8_t)0x55);
    _serial.flush();
}

void ModbusSlave::update() {
    // Accumula byte in arrivo
    while (_serial.available() && _len < BUF_SIZE) {
        _buf[_len++] = _serial.read();
        _lastByteTime = micros();
    }

    // Se abbiamo bytes e il bus è silenzioso da > FRAME_TIMEOUT → frame completo
    if (_len > 0 && (micros() - _lastByteTime) > FRAME_TIMEOUT_US) {
        processFrame();
        _len = 0;
    }
}

void ModbusSlave::processFrame() {
    // Frame minimo: addr(1) + fn(1) + data(n) + crc(2) → almeno 4 byte
    if (_len < 4) return;

    // Controlla indirizzo
    if (_buf[0] != _deviceId) return;

    // Controlla CRC
    uint16_t receivedCrc = (uint16_t)(_buf[_len-1] << 8) | _buf[_len-2];
    uint16_t computedCrc = crc16(_buf, _len - 2);
    if (receivedCrc != computedCrc) return;  // frame corrotto, ignora silenziosamente

    uint8_t  fnCode   = _buf[1];
    uint16_t startReg = ((uint16_t)_buf[2] << 8) | _buf[3];

    switch (fnCode) {
        case 0x03: {  // Read Holding Registers
            if (_len < 6) return;
            uint16_t count = ((uint16_t)_buf[4] << 8) | _buf[5];
            handleReadHoldingRegs(startReg, count);
            break;
        }
        case 0x06: {  // Write Single Register
            if (_len < 6) return;
            uint16_t value = ((uint16_t)_buf[4] << 8) | _buf[5];
            handleWriteSingleReg(startReg, value);
            break;
        }
        case 0x10: {  // Write Multiple Registers
            if (_len < 7) return;
            uint16_t count    = ((uint16_t)_buf[4] << 8) | _buf[5];
            uint8_t  byteCount = _buf[6];
            if (_len < (uint8_t)(7 + byteCount)) return;
            handleWriteMultipleRegs(startReg, count, &_buf[7]);
            break;
        }
        default:
            sendException(fnCode, 0x01);  // Function code not supported
            break;
    }
}

void ModbusSlave::handleReadHoldingRegs(uint16_t startReg, uint16_t count) {
    if (startReg + count > _numRegs) {
        sendException(0x03, 0x02);  // Illegal data address
        return;
    }
    // Risposta: addr + fn + byteCount + data[] + crc
    uint8_t resp[5 + count * 2];
    resp[0] = _deviceId;
    resp[1] = 0x03;
    resp[2] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++) {
        resp[3 + i*2]   = (uint8_t)(_regs[startReg + i] >> 8);
        resp[3 + i*2+1] = (uint8_t)(_regs[startReg + i] & 0xFF);
    }
    sendResponse(resp, 3 + count * 2);
}

void ModbusSlave::handleWriteSingleReg(uint16_t reg, uint16_t value) {
    if (reg >= _numRegs) {
        sendException(0x06, 0x02);
        return;
    }
    _regs[reg] = value;

    // Echo della richiesta come risposta (standard Modbus)
    sendResponse(_buf, 6);
}

void ModbusSlave::handleWriteMultipleRegs(uint16_t startReg, uint16_t count, uint8_t* data) {
    if (startReg + count > _numRegs) {
        sendException(0x10, 0x02);
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        _regs[startReg + i] = ((uint16_t)data[i*2] << 8) | data[i*2+1];
    }
    // Risposta: addr + fn + startReg + count + crc
    uint8_t resp[6];
    resp[0] = _deviceId;
    resp[1] = 0x10;
    resp[2] = (uint8_t)(startReg >> 8);
    resp[3] = (uint8_t)(startReg & 0xFF);
    resp[4] = (uint8_t)(count >> 8);
    resp[5] = (uint8_t)(count & 0xFF);
    sendResponse(resp, 6);
}

void ModbusSlave::sendResponse(uint8_t* data, uint8_t len) {
    uint16_t crc = crc16(data, len);
    setTx(true);
    delayMicroseconds(50);  // piccolo guard time prima di trasmettere
    _serial.write(data, len);
    _serial.write((uint8_t)(crc & 0xFF));   // CRC low byte
    _serial.write((uint8_t)(crc >> 8));     // CRC high byte
    _serial.flush();
    delayMicroseconds(50);
    setTx(false);
}

void ModbusSlave::sendException(uint8_t fnCode, uint8_t exCode) {
    uint8_t resp[3];
    resp[0] = _deviceId;
    resp[1] = fnCode | 0x80;
    resp[2] = exCode;
    sendResponse(resp, 3);
}

void ModbusSlave::setTx(bool transmit) {
    if (_dePin >= 0)
        digitalWrite(_dePin, transmit ? HIGH : LOW);
}

uint16_t ModbusSlave::crc16(uint8_t* buf, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else              crc >>= 1;
        }
    }
    return crc;
}