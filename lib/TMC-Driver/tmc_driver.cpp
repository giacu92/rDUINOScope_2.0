#include "tmc_driver.h"
#include <SPI.h>

// ── Helper template — evita il lambda con auto (ARM gcc < C++20) ──────────
template<typename T>
static void configureTMC(T& drv, uint16_t current) {
    drv.rms_current(current);
    drv.microsteps(TMC_MICROSTEPS);
    drv.pwm_autoscale(true);
    drv.en_spreadCycle(false);  // StealthChop di default
    drv.toff(4);
}

void TMCDriver::begin() {

#if defined(TMC_DRIVER_2208) || defined(TMC_DRIVER_2209)
    SerialTMC.begin(TMC_SERIAL_BAUDRATE, SERIAL_8N1);
    delay(100);

#elif defined(TMC_DRIVER_2130)
    pinMode(TMC_CS_RA,    OUTPUT); digitalWrite(TMC_CS_RA,    HIGH);
    pinMode(TMC_CS_DEC,   OUTPUT); digitalWrite(TMC_CS_DEC,   HIGH);
    pinMode(TMC_CS_FOCUS, OUTPUT); digitalWrite(TMC_CS_FOCUS, HIGH);
    SPI.begin(TMC_SPI_SCK, TMC_SPI_MISO, TMC_SPI_MOSI);
    _ra.begin();
    _dec.begin();
    _focus.begin();
#endif

    configureTMC(_ra,    TMC_CURRENT_RA);
    configureTMC(_dec,   TMC_CURRENT_DEC);
    configureTMC(_focus, TMC_CURRENT_FOCUS);

#if defined(TMC_DRIVER_2209)
    _raOk    = (_ra.version()    != 0);
    _decOk   = (_dec.version()   != 0);
    _focusOk = (_focus.version() != 0);
#elif defined(TMC_DRIVER_2130)
    _raOk    = (_ra.test_connection()    == 0);
    _decOk   = (_dec.test_connection()   == 0);
    _focusOk = (_focus.test_connection() == 0);
#else
    _raOk = _decOk = _focusOk = true;
#endif

#ifdef DEBUG_SERIAL
    Serial.printf("[TMC] RA=%s DEC=%s FOCUS=%s\n",
        _raOk    ? "OK" : "FAIL",
        _decOk   ? "OK" : "FAIL",
        _focusOk ? "OK" : "FAIL");
#endif
}

void TMCDriver::setCurrentRA   (uint16_t mA) { _ra.rms_current(mA);    }
void TMCDriver::setCurrentDEC  (uint16_t mA) { _dec.rms_current(mA);   }
void TMCDriver::setCurrentFocus(uint16_t mA) { _focus.rms_current(mA); }

void TMCDriver::setStealthChop(bool enable) {
    _ra.en_spreadCycle(!enable);
    _dec.en_spreadCycle(!enable);
    _focus.en_spreadCycle(!enable);
}