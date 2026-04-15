#pragma once
#include <Arduino.h>
#include "registers.h"

// Blackpill V3: LED su PC13, attivo basso
constexpr uint8_t LED_PIN    = PC13;
constexpr bool    LED_ACTIVE_LOW = true;

class LedStatus {
public:
    void begin() {
        pinMode(LED_PIN, OUTPUT);
        ledOff();
    }

    // Da chiamare nel loop()
    void update(uint16_t status) {
        uint32_t now = millis();

        switch (status) {

            case Status::IDLE:
                ledOn();
                break;

            case Status::SLEWING:
                // Blink 4Hz → 125ms on, 125ms off
                blink(now, 125, 125);
                break;

            case Status::TRACKING:
                // Blink 1Hz → 1000ms on, 1000ms off
                blink(now, 1000, 1000);
                break;

            case Status::ERROR:
                // 3 blink veloci (100ms) poi pausa 1s
                errorBlink(now, 3, 100, 1000);
                break;

            default:
                // STOP o sconosciuto → LED spento
                ledOff();
                break;
        }
    }

private:
    bool     _ledState     = false;
    uint32_t _lastChange   = 0;

    // Per error blink
    uint8_t  _errorCount   = 0;   // blink emessi nel burst corrente
    bool     _inPause      = false;

    void ledOn()  { digitalWrite(LED_PIN, LED_ACTIVE_LOW ? LOW  : HIGH); _ledState = true;  }
    void ledOff() { digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);  _ledState = false; }

    void blink(uint32_t now, uint32_t onMs, uint32_t offMs) {
        // Reset stato error se si arriva da Status::ERROR
        _errorCount = 0;
        _inPause    = false;

        uint32_t interval = _ledState ? onMs : offMs;
        if (now - _lastChange >= interval) {
            _ledState ? ledOff() : ledOn();
            _lastChange = now;
        }
    }

    void errorBlink(uint32_t now, uint8_t numBlinks, uint32_t blinkMs, uint32_t pauseMs) {
        if (_inPause) {
            // Aspetta la pausa lunga
            if (now - _lastChange >= pauseMs) {
                _inPause    = false;
                _errorCount = 0;
                _lastChange = now;
                ledOff();
            }
            return;
        }

        // Burst di blink
        if (now - _lastChange >= blinkMs) {
            if (!_ledState) {
                // Accendi solo se non abbiamo ancora finito i blink
                if (_errorCount < numBlinks) {
                    ledOn();
                    _lastChange = now;
                }
            } else {
                // Spegni
                ledOff();
                _errorCount++;
                _lastChange = now;

                if (_errorCount >= numBlinks) {
                    // Burst completato → entra in pausa
                    _inPause = true;
                }
            }
        }
    }
};