#include <Arduino.h>
#include <AccelStepper.h>
#include "config.h"
#include "modbus_slave.h"
#include "registers.h"
#include "led_status.h"
#include "tmc_driver.h"

#ifdef USE_ENCODER
  #include <Wire.h>
  #include <ams_as5048b.h>      // header con underscore, lowercase
  AMS_AS5048B encoderRA (0x40); // classe con prefisso AMS_
  AMS_AS5048B encoderDEC(0x41);
#endif

// ── Seriale Modbus ───────────────────────────────────────────────────────────
HardwareSerial SerialModbus(MODBUS_RX_PIN, MODBUS_TX_PIN);

// ── Seriale Trinamic ─────────────────────────────────────────────────────────
HardwareSerial SerialTMC(TMC_SERIAL_RX, TMC_SERIAL_TX);

// ── Registri Modbus ──────────────────────────────────────────────────────────
uint16_t regs[Reg::TOTAL] = {0};

// ── TMCDriver ────────────────────────────────────────────────────────────────
TMCDriver tmcDriver;

#ifdef USE_RS485
  ModbusSlave modbus(regs, Reg::TOTAL, 1, SerialModbus, PA15);
#else
  ModbusSlave modbus(regs, Reg::TOTAL, 1, SerialModbus, -1);
#endif

// ── AccelStepper ─────────────────────────────────────────────────────────────
AccelStepper axisRA (AccelStepper::DRIVER, RA_STEP,  RA_DIR);
AccelStepper axisDEC(AccelStepper::DRIVER, DEC_STEP, DEC_DIR);

// ── Stato globale ─────────────────────────────────────────────────────────────
bool trackingActive = false;
bool gotoInProgress = false;
bool followTrackingEnabled = false;
bool softStopInProgress = false;
bool motorsEnabled = true;
bool manualJogActive = false;
uint16_t trackingMode = TrackingMode::SIDEREAL;
uint16_t activeJogAxis = JogAxis::RA_AXIS;
LedStatus ledStatus;

uint32_t currentRAArcsec100  = 0;
int32_t  currentDECArcsec100 = 0;
uint32_t targetRAArcsec100   = 0;
int32_t  targetDECArcsec100  = 0;


// ── Helpers coordinate ───────────────────────────────────────────────────────
inline int32_t arcsec100ToSteps(int32_t arcsec100) {
    return (int32_t)((float)arcsec100 / 129600000.0f * STEPS_PER_REV);
}

inline int32_t stepsToArcsec100(int32_t steps) {
    return (int32_t)((float)steps * 129600000.0f / STEPS_PER_REV);
}

inline uint32_t normalizeRAArcsec100(int32_t arcsec100) {
    int32_t normalized = arcsec100 % 129600000L;
    if (normalized < 0) normalized += 129600000L;
    return (uint32_t)normalized;
}

float trackingRateForMode(uint16_t mode) {
    switch (mode) {
        case TrackingMode::LUNAR:
            return SIDEREAL_RATE_HZ * 0.963f;
        case TrackingMode::SOLAR:
            return (float)STEPS_PER_REV / 86400.0f;
        case TrackingMode::SIDEREAL:
        default:
            return SIDEREAL_RATE_HZ;
    }
}

float jogSpeedForProfile(uint16_t profile) {
    switch (profile) {
        case JogSpeed::GUIDE:
            return max(10.0f, SIDEREAL_RATE_HZ * 8.0f);
        case JogSpeed::CENTER:
            return MAX_SPEED * 0.25f;
        case JogSpeed::SLEW:
        default:
            return MAX_SPEED;
    }
}

// Motori / emergency stop
void setMotorOutputsEnabled(bool enabled) {
    // TMC EN is active LOW.
    digitalWrite(RA_EN,  enabled ? LOW : HIGH);
    digitalWrite(DEC_EN, enabled ? LOW : HIGH);
}

void immediateMotorStop() {
    long raNow  = axisRA.currentPosition();
    long decNow = axisDEC.currentPosition();

    axisRA.setCurrentPosition(raNow);
    axisDEC.setCurrentPosition(decNow);
    axisRA.moveTo(raNow);
    axisDEC.moveTo(decNow);

    regs[Reg::REQ_TRACKING_ENABLE] = 0;
    trackingActive = false;
    gotoInProgress = false;
    followTrackingEnabled = false;
    softStopInProgress = false;
    manualJogActive = false;
    motorsEnabled = false;
    setMotorOutputsEnabled(false);

    currentRAArcsec100  = normalizeRAArcsec100(stepsToArcsec100(raNow));
    currentDECArcsec100 = stepsToArcsec100(decNow);
    targetRAArcsec100   = currentRAArcsec100;
    targetDECArcsec100  = currentDECArcsec100;
    encode32(currentRAArcsec100,  regs[Reg::RES_CURRENT_RA_HI],  regs[Reg::RES_CURRENT_RA_LO]);
    encode32Signed(currentDECArcsec100, regs[Reg::RES_CURRENT_DEC_HI], regs[Reg::RES_CURRENT_DEC_LO]);

    regs[Reg::RES_STATUS]     = Status::MOTORS_DISABLED;
    regs[Reg::RES_ERROR_CODE] = 0x00;
}

void saveCurrentStepperPosition() {
    currentRAArcsec100  = normalizeRAArcsec100(stepsToArcsec100(axisRA.currentPosition()));
    currentDECArcsec100 = stepsToArcsec100(axisDEC.currentPosition());
    targetRAArcsec100   = currentRAArcsec100;
    targetDECArcsec100  = currentDECArcsec100;
    encode32(currentRAArcsec100,  regs[Reg::RES_CURRENT_RA_HI],  regs[Reg::RES_CURRENT_RA_LO]);
    encode32Signed(currentDECArcsec100, regs[Reg::RES_CURRENT_DEC_HI], regs[Reg::RES_CURRENT_DEC_LO]);
}

void controlledMotorStop() {
    if (motorsEnabled) {
        setMotorOutputsEnabled(true);
    }

    regs[Reg::REQ_TRACKING_ENABLE] = 0;
    trackingActive = false;
    gotoInProgress = false;
    followTrackingEnabled = false;
    manualJogActive = false;
    softStopInProgress = true;

    axisRA.setMaxSpeed(MAX_SPEED);
    axisRA.setAcceleration(ACCELERATION);
    axisDEC.setMaxSpeed(MAX_SPEED);
    axisDEC.setAcceleration(ACCELERATION);
    axisRA.stop();
    axisDEC.stop();

    if (axisRA.distanceToGo() == 0 && axisDEC.distanceToGo() == 0) {
        softStopInProgress = false;
        saveCurrentStepperPosition();
        regs[Reg::RES_STATUS] = Status::IDLE;
    }
}

bool isHardwiredStopActive() {
    return digitalRead(ESTOP_PIN) == ESTOP_ACTIVE_STATE;
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
    if (!motorsEnabled) {
        regs[Reg::RES_STATUS] = Status::MOTORS_DISABLED;
        return;
    }

    setMotorOutputsEnabled(true);
    softStopInProgress = false;
    manualJogActive = false;
    float rate = trackingRateForMode(trackingMode);
    axisRA.setMaxSpeed(rate);
    axisRA.setAcceleration(10.0f);
    axisRA.moveTo(axisRA.currentPosition() - (long)STEPS_PER_REV * 1000L);
    trackingActive = true;
    #ifdef DEBUG_SERIAL
        Serial.printf("[TRACK] mode=%u rate=%.3f steps/s\n", trackingMode, rate);
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

// ── Target da registri Modbus ────────────────────────────────────────────────
void readTargetRegisters(uint32_t& ra, int32_t& dec) {
    ra  = normalizeRAArcsec100(
        decode32(regs[Reg::REQ_TARGET_RA_HI], regs[Reg::REQ_TARGET_RA_LO]));
    dec = decode32Signed(regs[Reg::REQ_TARGET_DEC_HI], regs[Reg::REQ_TARGET_DEC_LO]);
}

bool targetRegistersChanged() {
    uint32_t ra;
    int32_t dec;
    readTargetRegisters(ra, dec);
    return ra != targetRAArcsec100 || dec != targetDECArcsec100;
}

void moveToTargetRegisters(bool trackOnArrival, const char* commandName) {
    if (!motorsEnabled) {
        regs[Reg::RES_STATUS] = Status::MOTORS_DISABLED;
        #ifdef DEBUG_SERIAL
            Serial.printf("[CMD] %s ignored: motors disabled\n", commandName);
        #endif
        return;
    }

    uint32_t ra;
    int32_t dec;
    readTargetRegisters(ra, dec);

    targetRAArcsec100  = ra;
    targetDECArcsec100 = dec;
    setMotorOutputsEnabled(true);
    gotoInProgress = trackOnArrival;
    softStopInProgress = false;
    manualJogActive = false;

    stopTracking();
    axisRA.setMaxSpeed(MAX_SPEED);
    axisRA.setAcceleration(ACCELERATION);
    axisDEC.setMaxSpeed(MAX_SPEED);
    axisDEC.setAcceleration(ACCELERATION);
    axisRA.moveTo(arcsec100ToSteps(targetRAArcsec100));
    axisDEC.moveTo(arcsec100ToSteps(targetDECArcsec100));
    regs[Reg::RES_STATUS] = Status::SLEWING;

    #ifdef DEBUG_SERIAL
        Serial.printf("[CMD] %s RA=%lu DEC=%ld\n",
                      commandName, (unsigned long)ra, (long)dec);
    #endif
}

// ── Comandi mount Milestone 0 ────────────────────────────────────────────────
void setTrackingFromRegisters() {
    trackingMode = regs[Reg::REQ_TRACKING_MODE];
    if (trackingMode > TrackingMode::SOLAR) {
        trackingMode = TrackingMode::SIDEREAL;
    }

    if (regs[Reg::REQ_TRACKING_ENABLE] != 0) {
        startTracking();
    } else {
        controlledMotorStop();
    }

    #ifdef DEBUG_SERIAL
        Serial.printf("[CMD] SET_TRACKING enable=%u mode=%u\n",
                      regs[Reg::REQ_TRACKING_ENABLE], trackingMode);
    #endif
}

void setMotorsFromRegisters() {
    motorsEnabled = regs[Reg::REQ_MOTORS_ENABLE] != 0;

    if (motorsEnabled) {
        setMotorOutputsEnabled(true);
        manualJogActive = false;
        softStopInProgress = false;
        if (!trackingActive && !gotoInProgress) {
            regs[Reg::RES_STATUS] = Status::IDLE;
        }
    } else {
        long raNow  = axisRA.currentPosition();
        long decNow = axisDEC.currentPosition();

        trackingActive = false;
        gotoInProgress = false;
        followTrackingEnabled = false;
        softStopInProgress = false;
        manualJogActive = false;
        axisRA.moveTo(raNow);
        axisDEC.moveTo(decNow);
        setMotorOutputsEnabled(false);
        saveCurrentStepperPosition();
        regs[Reg::RES_STATUS] = Status::MOTORS_DISABLED;
    }

    #ifdef DEBUG_SERIAL
        Serial.printf("[CMD] SET_MOTORS enable=%u\n", regs[Reg::REQ_MOTORS_ENABLE]);
    #endif
}

void startJogFromRegisters() {
    if (!motorsEnabled) {
        regs[Reg::RES_STATUS] = Status::MOTORS_DISABLED;
        return;
    }

    uint16_t axis = regs[Reg::REQ_JOG_AXIS];
    uint16_t direction = regs[Reg::REQ_JOG_DIRECTION];
    float speed = jogSpeedForProfile(regs[Reg::REQ_JOG_SPEED]);
    long jogDistance = (direction == JogDirection::POSITIVE)
                         ? (long)STEPS_PER_REV * 1000L
                         : -(long)STEPS_PER_REV * 1000L;

    controlledMotorStop();
    softStopInProgress = false;
    setMotorOutputsEnabled(true);

    if (axis == JogAxis::DEC_AXIS) {
        activeJogAxis = JogAxis::DEC_AXIS;
        axisDEC.setMaxSpeed(speed);
        axisDEC.setAcceleration(ACCELERATION);
        axisDEC.moveTo(axisDEC.currentPosition() + jogDistance);
    } else {
        activeJogAxis = JogAxis::RA_AXIS;
        axisRA.setMaxSpeed(speed);
        axisRA.setAcceleration(ACCELERATION);
        axisRA.moveTo(axisRA.currentPosition() + jogDistance);
    }

    manualJogActive = true;
    regs[Reg::RES_STATUS] = Status::MANUAL_JOG;

    #ifdef DEBUG_SERIAL
        Serial.printf("[CMD] JOG_START axis=%u dir=%u speedProfile=%u speed=%.1f\n",
                      axis, direction, regs[Reg::REQ_JOG_SPEED], speed);
    #endif
}

void stopJog() {
    if (!manualJogActive) return;

    if (activeJogAxis == JogAxis::DEC_AXIS) {
        axisDEC.stop();
    } else {
        axisRA.stop();
    }

    manualJogActive = false;
    softStopInProgress = true;

    #ifdef DEBUG_SERIAL
        Serial.println("[CMD] JOG_STOP");
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
            regs[Reg::RES_STATUS]     = Status::ERROR;
            regs[Reg::RES_ERROR_CODE] = 0x01;  // position fault
            encode32(normalizeRAArcsec100(stepsToArcsec100(encRA)),
                     regs[Reg::RES_CURRENT_RA_HI], regs[Reg::RES_CURRENT_RA_LO]);
            encode32Signed(stepsToArcsec100(encDEC), regs[Reg::RES_CURRENT_DEC_HI], regs[Reg::RES_CURRENT_DEC_LO]);
            return;
        }

        if (!trackingActive) {
            currentRAArcsec100 = normalizeRAArcsec100(stepsToArcsec100(encRA));
        }
        currentDECArcsec100 = stepsToArcsec100(encDEC);

        encode32(currentRAArcsec100,  regs[Reg::RES_CURRENT_RA_HI],  regs[Reg::RES_CURRENT_RA_LO]);
        encode32Signed(currentDECArcsec100, regs[Reg::RES_CURRENT_DEC_HI], regs[Reg::RES_CURRENT_DEC_LO]);
    }
    #else
    {
        if (!trackingActive && axisRA.distanceToGo() != 0) {
            currentRAArcsec100 = normalizeRAArcsec100(
                stepsToArcsec100(axisRA.currentPosition()));
        }
        if (!trackingActive && axisDEC.distanceToGo() != 0) {
            currentDECArcsec100 = stepsToArcsec100(axisDEC.currentPosition());
        }

        encode32(currentRAArcsec100,
                 regs[Reg::RES_CURRENT_RA_HI], regs[Reg::RES_CURRENT_RA_LO]);
        encode32Signed(currentDECArcsec100,
                       regs[Reg::RES_CURRENT_DEC_HI], regs[Reg::RES_CURRENT_DEC_LO]);
    }
    #endif

    bool slewing = (!trackingActive && !manualJogActive && axisRA.distanceToGo()  != 0)
                ||  (!manualJogActive && axisDEC.distanceToGo() != 0);

    if      (!motorsEnabled)    regs[Reg::RES_STATUS] = Status::MOTORS_DISABLED;
    else if (manualJogActive)   regs[Reg::RES_STATUS] = Status::MANUAL_JOG;
    else if (slewing)           regs[Reg::RES_STATUS] = Status::SLEWING;
    else if (trackingActive)    regs[Reg::RES_STATUS] = Status::TRACKING;
    else                        regs[Reg::RES_STATUS] = Status::IDLE;

    regs[Reg::RES_ERROR_CODE] = 0x00;
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    #ifdef DEBUG_SERIAL
        Serial.begin(115200);
        delay(500);
        Serial.println("[BOOT] Telescope controller starting...");
    #endif

    modbus.begin(MODBUS_BAUDRATE);
    ledStatus.begin();

    tmcDriver.begin();
    // Segnala errore su LED se un driver non risponde
    if (!tmcDriver.isRAok() || !tmcDriver.isDECok()) {
        regs[Reg::RES_STATUS]     = Status::ERROR;
        regs[Reg::RES_ERROR_CODE] = 0x02;  // driver fault
    }

    pinMode(ESTOP_PIN, INPUT_PULLUP);
    pinMode(RA_EN,  OUTPUT);
    pinMode(DEC_EN, OUTPUT);
    regs[Reg::REQ_TRACKING_ENABLE] = 0;
    regs[Reg::REQ_TRACKING_MODE] = TrackingMode::SIDEREAL;
    regs[Reg::REQ_MOTORS_ENABLE] = 1;
    regs[Reg::REQ_JOG_SPEED] = JogSpeed::CENTER;
    regs[Reg::RES_STM32_FW_VERSION] = STM32_FIRMWARE_VERSION;
    motorsEnabled = true;
    trackingMode = regs[Reg::REQ_TRACKING_MODE];
    setMotorOutputsEnabled(true);

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

    regs[Reg::RES_STATUS] = Status::IDLE;

    #ifdef DEBUG_SERIAL
        Serial.println("[BOOT] Ready");
    #endif
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // 1. Modbus
    modbus.update();

    if (isHardwiredStopActive()) {
        immediateMotorStop();
        ledStatus.update(regs[Reg::RES_STATUS]);
        return;
    }

    if (trackingActive && regs[Reg::REQ_TRACKING_ENABLE] == 0) {
        controlledMotorStop();
    }

    // 2. Comandi
    if (regs[Reg::REQ_COMMAND_PENDING] != 0) {
        uint16_t cmd = regs[Reg::REQ_COMMAND];
        regs[Reg::REQ_COMMAND_PENDING] = 0;

        switch (cmd) {

            case Cmd::GOTO:
                followTrackingEnabled = false;
                moveToTargetRegisters(true, "GOTO");
                break;

            case Cmd::FOLLOW_TARGET:
                followTrackingEnabled = trackingActive;
                if (targetRegistersChanged()) {
                    moveToTargetRegisters(followTrackingEnabled, "FOLLOW");
                }
                break;

            case Cmd::STOP:
                followTrackingEnabled = false;
                controlledMotorStop();
                #ifdef DEBUG_SERIAL
                    Serial.println("[CMD] STOP");
                #endif
                break;

            case Cmd::SET_TRACKING:
                setTrackingFromRegisters();
                break;

            case Cmd::SET_MOTORS:
                setMotorsFromRegisters();
                break;

            case Cmd::JOG_START:
                startJogFromRegisters();
                break;

            case Cmd::JOG_STOP:
                stopJog();
                break;

            case Cmd::SYNC: {
                currentRAArcsec100 = normalizeRAArcsec100(
                    decode32(regs[Reg::REQ_TARGET_RA_HI],  regs[Reg::REQ_TARGET_RA_LO]));
                currentDECArcsec100 = decode32Signed(regs[Reg::REQ_TARGET_DEC_HI],
                                                     regs[Reg::REQ_TARGET_DEC_LO]);
                targetRAArcsec100  = currentRAArcsec100;
                targetDECArcsec100 = currentDECArcsec100;
                int32_t raSteps  = arcsec100ToSteps(currentRAArcsec100);
                int32_t decSteps = arcsec100ToSteps(currentDECArcsec100);
                gotoInProgress = false;
                followTrackingEnabled = false;
                softStopInProgress = false;
                setMotorOutputsEnabled(true);
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

    if (followTrackingEnabled && targetRegistersChanged()) {
        moveToTargetRegisters(followTrackingEnabled, "FOLLOW");
    }

    // 3. Avvia tracking automatico al termine del GOTO
    bool isMoving = (axisDEC.distanceToGo() != 0) ||
                    (!trackingActive && axisRA.distanceToGo() != 0);
    if (gotoInProgress && !isMoving) {
        gotoInProgress = false;
        currentRAArcsec100  = targetRAArcsec100;
        currentDECArcsec100 = targetDECArcsec100;
        startTracking();
        #ifdef DEBUG_SERIAL
            Serial.println("[GOTO] Complete, tracking started");
        #endif
    }
    // 4. Motori — chiamare il più spesso possibile
    axisRA.run();
    axisDEC.run();

    if (softStopInProgress &&
        axisRA.distanceToGo() == 0 &&
        axisDEC.distanceToGo() == 0) {
        softStopInProgress = false;
        saveCurrentStepperPosition();
        regs[Reg::RES_STATUS] = Status::IDLE;
        #ifdef DEBUG_SERIAL
            Serial.println("[CMD] STOP complete, position saved");
        #endif
    }

    // 5. Posizione e stato nei registri
    updatePositionRegisters();

    // 6. LED
    ledStatus.update(regs[Reg::RES_STATUS]);
}
