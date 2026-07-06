// calibration.h — persists each motor's learned calibration (travel span,
// opening encoder sign, last endpoint state) to ESP32 flash (NVS) so position
// tracking survives reboots. Saves happen automatically whenever a motor
// reports its calibration changed (endpoint stalls, span measurements).
#pragma once
#include <Preferences.h>
#include "config.h"
#include "control.h"

Preferences calibStore;

void saveMotorCalibration(Motor& m, const char* spanKey, const char* signKey, const char* stateKey) {
  calibStore.putLong(spanKey, m.getTravelSpan());
  calibStore.putInt(signKey, m.getOpenSign());
  Motor::State s = m.getState();
  int st = (s == Motor::OPEN || s == Motor::CLOSED) ? (int)s : -1;
  calibStore.putInt(stateKey, st);
  Serial.print("[CAL] Saved: span=");
  Serial.print(m.getTravelSpan());
  Serial.print(" sign=");
  Serial.print(m.getOpenSign());
  Serial.print(" state=");
  Serial.println(st);
}

void loadMotorCalibration(Motor& m, const char* spanKey, const char* signKey, const char* stateKey) {
  long span = calibStore.getLong(spanKey, 0);
  int sign = calibStore.getInt(signKey, 0);
  int st = calibStore.getInt(stateKey, -1);
  if (span > 0 || st >= 0) {
    m.restoreCalibration(span, sign, st);
    Serial.print("[CAL] Restored: span=");
    Serial.print(span);
    Serial.print(" sign=");
    Serial.print(sign);
    Serial.print(" state=");
    Serial.println(st);
  }
}

void calibrationBegin() {
  calibStore.begin("curtainbot", false);
  loadMotorCalibration(motor1, "span1", "sign1", "state1");
  loadMotorCalibration(motor2, "span2", "sign2", "state2");
}

// Call every loop(); writes to flash only when something actually changed.
void calibrationUpdate() {
  if (motor1.consumeCalibDirty()) saveMotorCalibration(motor1, "span1", "sign1", "state1");
  if (motor2.consumeCalibDirty()) saveMotorCalibration(motor2, "span2", "sign2", "state2");
}

// Forget everything (the "recalibrate" command). The next full close + full
// open re-learns span, direction, and position.
void calibrationClear() {
  motor1.clearCalibration();
  motor2.clearCalibration();
  calibStore.clear();
  Serial.println("[CAL] Calibration cleared - do a full close then full open to re-learn");
}
