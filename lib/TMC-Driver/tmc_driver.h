#pragma once
#include <Arduino.h>
#include <TMCStepper.h>
#include "config.h"

// extern pura — nessun argomento, nessuna inizializzazione
// L'istanza è definita in main.cpp: HardwareSerial SerialTMC(PA12, PA11);
extern HardwareSerial SerialTMC;

class TMCDriver {
public:
    void begin();

    bool isRAok()    const { return _raOk;    }
    bool isDECok()   const { return _decOk;   }
    bool isFocusOk() const { return _focusOk; }

    void setCurrentRA   (uint16_t mA);
    void setCurrentDEC  (uint16_t mA);
    void setCurrentFocus(uint16_t mA);
    void setStealthChop(bool enable);

private:
    bool _raOk = false, _decOk = false, _focusOk = false;

#if defined(TMC_DRIVER_2208)
    TMC2208Stepper _ra   {&SerialTMC, 0.11f};
    TMC2208Stepper _dec  {&SerialTMC, 0.11f};
    TMC2208Stepper _focus{&SerialTMC, 0.11f};

#elif defined(TMC_DRIVER_2209)
    TMC2209Stepper _ra   {&SerialTMC, 0.11f, 0};
    TMC2209Stepper _dec  {&SerialTMC, 0.11f, 1};
    TMC2209Stepper _focus{&SerialTMC, 0.11f, 2};

#elif defined(TMC_DRIVER_2130)
    TMC2130Stepper _ra   {TMC_CS_RA,    0.11f};
    TMC2130Stepper _dec  {TMC_CS_DEC,   0.11f};
    TMC2130Stepper _focus{TMC_CS_FOCUS, 0.11f};

#else
    #error "Definire TMC_DRIVER_2208, TMC_DRIVER_2209 o TMC_DRIVER_2130 in config.h"
#endif
};