#include <Arduino.h>
#include <AccelStepper.h>
#include "config.h"
#include "modbus_slave.h"
#include "registers.h"
#include "led_status.h"

#ifdef USE_ENCODER
  #include <Wire.h>
  #include <ams_as5048b.h>      // header con underscore, lowercase
  AMS_AS5048B encoderRA (0x40); // classe con prefisso AMS_
  AMS_AS5048B encoderDEC(0x41);
#endif

// ── Seriale Modbus ───────────────────────────────────────────────────────────
HardwareSerial SerialModbus(PA10, PA9);

// ── Registri Modbus ──────────────────────────────────────────────────────────
uint16_t regs[Reg::TOTAL] = {0};


#ifdef USE_RS485
  ModbusSlave modbus(regs, Reg::TOTAL, 1, SerialModbus, PA8);
#else
  ModbusSlave modbus(regs, Reg::TOTAL, 1, SerialModbus, -1);
#endif

// ── Pin motori ───────────────────────────────────────────────────────────────
constexpr uint8_t RA_STEP  = PB3;
constexpr uint8_t RA_DIR   = PB4;
constexpr uint8_t RA_EN    = PB5;
constexpr uint8_t DEC_STEP = PB6;
constexpr uint8_t DEC_DIR  = PB7;
constexpr uint8_t DEC_EN   = PB8;

// ── AccelStepper ─────────────────────────────────────────────────────────────
AccelStepper axisRA (AccelStepper::DRIVER, RA_STEP,  RA_DIR);
AccelStepper axisDEC(AccelStepper::DRIVER, DEC_STEP, DEC_DIR);

// ── Stato globale ─────────────────────────────────────────────────────────────
bool trackingActive = false;
LedStatus ledStatus;


// ── Helpers coordinate ───────────────────────────────────────────────────────
inline int32_t arcsec100ToSteps(uint32_t arcsec100) {
    return (int32_t)((float)arcsec100 / 129600000.0f * STEPS_PER_REV);
}

inline uint32_t stepsToArcsec100(int32_t steps) {
    return (uint32_t)((float)steps * 129600000.0f / STEPS_PER_REV);
}

// ── Encoder: lettura con gestione wrap-around ────────────────────────────────
#ifdef USE_ENCODER
int32_t encoderToSteps(AMS_AS5048B& enc, float& lastAngle, int32_t& accumSteps) {
    float angle = enc.angleR(U_DEG, true);  // true = moving average
    float delta = angle - lastAngle;

    // Wrap-around 0°/360°
    if      (delta >  180.0f) delta -= 360.0f;
    else if (delta < -180.0f) delta += 360.0f;

    accumSteps += (int32_t)(delta / 360.0f * STEPS_PER_REV);
    lastAngle   = angle;
    return accumSteps;
}
#endif

// ── Tracking siderale ────────────────────────────────────────────────────────
void startTracking() {
    axisRA.setMaxSpeed(SIDEREAL_RATE_HZ * 1.1f);
    axisRA.setAcceleration(10.0f);
    axisRA.moveTo(axisRA.currentPosition() + (long)STEPS_PER_REV * 100L);
    trackingActive = true;
    #ifdef DEBUG_SERIAL
        Serial.println("[TRACK] Sidereal tracking started");
    #endif
}

void stopTracking() {
    axisRA.setMaxSpeed(MAX_SPEED);
    axisRA.setAcceleration(ACCELERATION);
    axisRA.stop();
    trackingActive = false;
    #ifdef DEBUG_SERIAL
        Serial.println("[TRACK] Stopped");
    #endif
}

// ── Aggiornamento posizione e stato ──────────────────────────────────────────
void updatePositionRegisters() {

    #ifdef USE_ENCODER
    {
        static float   lastAngleRA  = 0.0f;
        static float   lastAngleDEC = 0.0f;
        static int32_t accumRA      = 0;
        static int32_t accumDEC     = 0;
        static bool    initialized  = false;

        if (!initialized) {
            lastAngleRA  = encoderRA.angleR(U_DEG,  true);
            lastAngleDEC = encoderDEC.angleR(U_DEG, true);
            initialized  = true;
        }

        int32_t encRA  = encoderToSteps(encoderRA,  lastAngleRA,  accumRA);
        int32_t encDEC = encoderToSteps(encoderDEC, lastAngleDEC, accumDEC);
        int32_t errRA  = axisRA.currentPosition()  - encRA;
        int32_t errDEC = axisDEC.currentPosition() - encDEC;

        #ifdef DEBUG_SERIAL
        {
            static uint32_t lastPrint = 0;
            if (millis() - lastPrint > 500) {
                Serial.printf("[ENC] RA  step=%ld enc=%ld err=%ld\n",
                              axisRA.currentPosition(),  encRA,  errRA);
                Serial.printf("[ENC] DEC step=%ld enc=%ld err=%ld\n",
                              axisDEC.currentPosition(), encDEC, errDEC);
                lastPrint = millis();
            }
        }
        #endif

        // Fault: fermo ma encoder diverge
        bool shouldBeStill = !trackingActive
                          && axisRA.distanceToGo()  == 0
                          && axisDEC.distanceToGo() == 0;
        if (shouldBeStill &&
           (abs(errRA)  > ENCODER_ERROR_THRESHOLD ||
            abs(errDEC) > ENCODER_ERROR_THRESHOLD)) {
            regs[Reg::STATUS]     = Status::ERROR;
            regs[Reg::ERROR_CODE] = 0x01;  // position fault
            encode32(stepsToArcsec100(encRA),  regs[Reg::CURRENT_RA_HI],  regs[Reg::CURRENT_RA_LO]);
            encode32(stepsToArcsec100(encDEC), regs[Reg::CURRENT_DEC_HI], regs[Reg::CURRENT_DEC_LO]);
            return;
        }

        encode32(stepsToArcsec100(encRA),  regs[Reg::CURRENT_RA_HI],  regs[Reg::CURRENT_RA_LO]);
        encode32(stepsToArcsec100(encDEC), regs[Reg::CURRENT_DEC_HI], regs[Reg::CURRENT_DEC_LO]);
    }
    #else
    {
        encode32(stepsToArcsec100(axisRA.currentPosition()),
                 regs[Reg::CURRENT_RA_HI], regs[Reg::CURRENT_RA_LO]);
        encode32(stepsToArcsec100(axisDEC.currentPosition()),
                 regs[Reg::CURRENT_DEC_HI], regs[Reg::CURRENT_DEC_LO]);
    }
    #endif

    bool slewing = (!trackingActive && axisRA.distanceToGo()  != 0)
                ||  (axisDEC.distanceToGo() != 0);

    if      (slewing)        regs[Reg::STATUS] = Status::SLEWING;
    else if (trackingActive) regs[Reg::STATUS] = Status::TRACKING;
    else                     regs[Reg::STATUS] = Status::IDLE;

    regs[Reg::ERROR_CODE] = 0x00;
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    #ifdef DEBUG_SERIAL
        Serial.begin(115200);
        delay(500);
        Serial.println("[BOOT] Telescope controller starting...");
    #endif

    modbus.begin(9600);
    ledStatus.begin();

    pinMode(RA_EN,  OUTPUT); digitalWrite(RA_EN,  LOW);
    pinMode(DEC_EN, OUTPUT); digitalWrite(DEC_EN, LOW);

    axisRA.setMaxSpeed(MAX_SPEED);
    axisRA.setAcceleration(ACCELERATION);
    axisDEC.setMaxSpeed(MAX_SPEED);
    axisDEC.setAcceleration(ACCELERATION);

    #ifdef USE_ENCODER
        Wire.begin();
        encoderRA.begin();
        encoderDEC.begin();
        encoderRA.setClockWise(true);
        encoderDEC.setClockWise(true);
        #ifdef DEBUG_SERIAL
            Serial.println("[ENC] AS5048B initialized");
        #endif
    #endif

    regs[Reg::STATUS] = Status::IDLE;

    #ifdef DEBUG_SERIAL
        Serial.println("[BOOT] Ready");
    #endif
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Modbus
    modbus.update();

    // 2. Comandi
    static uint16_t lastCmd = Cmd::IDLE;
    uint16_t cmd = regs[Reg::COMMAND];

    if (cmd != lastCmd) {
        lastCmd = cmd;
        switch (cmd) {

            case Cmd::GOTO: {
                uint32_t ra  = decode32(regs[Reg::TARGET_RA_HI],  regs[Reg::TARGET_RA_LO]);
                uint32_t dec = decode32(regs[Reg::TARGET_DEC_HI], regs[Reg::TARGET_DEC_LO]);
                stopTracking();
                axisRA.setMaxSpeed(MAX_SPEED);
                axisRA.setAcceleration(ACCELERATION);
                axisRA.moveTo(arcsec100ToSteps(ra));
                axisDEC.moveTo(arcsec100ToSteps(dec));
                regs[Reg::STATUS] = Status::SLEWING;
                #ifdef DEBUG_SERIAL
                    Serial.printf("[CMD] GOTO RA=%lu DEC=%lu\n", ra, dec);
                #endif
                break;
            }

            case Cmd::STOP:
                stopTracking();
                axisDEC.stop();
                regs[Reg::STATUS] = Status::IDLE;
                #ifdef DEBUG_SERIAL
                    Serial.println("[CMD] STOP");
                #endif
                break;

            case Cmd::SYNC: {
                int32_t raSteps  = arcsec100ToSteps(
                    decode32(regs[Reg::TARGET_RA_HI],  regs[Reg::TARGET_RA_LO]));
                int32_t decSteps = arcsec100ToSteps(
                    decode32(regs[Reg::TARGET_DEC_HI], regs[Reg::TARGET_DEC_LO]));
                axisRA.setCurrentPosition(raSteps);
                axisDEC.setCurrentPosition(decSteps);
                startTracking();
                #ifdef DEBUG_SERIAL
                    Serial.printf("[CMD] SYNC RA=%ld DEC=%ld steps\n", raSteps, decSteps);
                #endif
                break;
            }
        }
    }

    // 3. Avvia tracking automatico al termine del GOTO
    static bool wasMoving = false;
    bool isMoving = (axisDEC.distanceToGo() != 0) ||
                    (!trackingActive && axisRA.distanceToGo() != 0);
    if (wasMoving && !isMoving) {
        startTracking();
        #ifdef DEBUG_SERIAL
            Serial.println("[GOTO] Complete, tracking started");
        #endif
    }
    wasMoving = isMoving;

    // 4. Motori — chiamare il più spesso possibile
    axisRA.run();
    axisDEC.run();

    // 5. Posizione e stato nei registri
    updatePositionRegisters();

    // 6. LED
    ledStatus.update(regs[Reg::STATUS]);
}