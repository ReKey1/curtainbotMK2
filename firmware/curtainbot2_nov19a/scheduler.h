// scheduler.h — NTP timekeeping + morning/night curtain routines.
// schedulerUpdate() is called every loop() iteration. Each routine runs in
// two quiet stages: ROUTINE_LEAD_MIN minutes before the set time the curtains
// move to ROUTINE_MID_PCT, then fully open/close exactly at the set time.
// Each stage fires at most once per matching minute.
#pragma once
#include <time.h>
#include "config.h"
#include "control.h"

struct CurtainSchedule {
  int  hour;
  int  min;
  bool enabled;
  long lastFiredKey;     // yday*1440 + minute-of-day of the last main firing
  long lastPreFiredKey;  // same, for the pre-stage

  long keyOf(const struct tm& t) {
    return (long)t.tm_yday * 1440L + t.tm_hour * 60 + t.tm_min;
  }

  // True exactly once per matching minute while enabled.
  bool due(const struct tm& t) {
    if (!enabled) return false;
    if (t.tm_hour != hour || t.tm_min != min) return false;
    long key = keyOf(t);
    if (key == lastFiredKey) return false;
    lastFiredKey = key;
    return true;
  }

  // True exactly once when the clock hits (hour:min - leadMin), wrapping
  // around midnight.
  bool preDue(const struct tm& t, int leadMin) {
    if (!enabled) return false;
    int preMinuteOfDay = (hour * 60 + min - leadMin + 1440) % 1440;
    if (t.tm_hour * 60 + t.tm_min != preMinuteOfDay) return false;
    long key = keyOf(t);
    if (key == lastPreFiredKey) return false;
    lastPreFiredKey = key;
    return true;
  }
};

CurtainSchedule morningSched = { DEFAULT_MORNING_HOUR, DEFAULT_MORNING_MIN, DEFAULT_MORNING_ENABLED, -1, -1 };
CurtainSchedule nightSched   = { DEFAULT_NIGHT_HOUR,   DEFAULT_NIGHT_MIN,   DEFAULT_NIGHT_ENABLED,   -1, -1 };

void schedulerBegin() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

// Non-blocking time fetch; false until the first NTP sync completes.
bool getTimeNow(struct tm& t) {
  return getLocalTime(&t, 0);
}

// Pre-stage: park the curtain at the halfway point — but only if that is in
// the direction the routine is headed (never back away from it), and only
// when position is calibrated (a full move at the set time covers the rest).
void routinePreStage(Motor& m, bool opening) {
  float pct = m.positionPercent();
  if (pct < 0) return;
  if (opening ? (pct >= ROUTINE_MID_PCT) : (pct <= ROUTINE_MID_PCT)) return;
  m.moveToPercent(ROUTINE_MID_PCT);
}

// Main stage: drive fully into the endpoint (skipped if already there).
void routineFinal(Motor& m, bool opening) {
  if (opening) {
    if (m.getState() != Motor::OPEN) m.moveOpen();
  } else {
    if (m.getState() != Motor::CLOSED) m.moveClosed();
  }
}

void schedulerUpdate() {
  struct tm t;
  if (!getTimeNow(t)) return;  // clock not synced yet

  if (morningSched.preDue(t, ROUTINE_LEAD_MIN)) {
    Serial.println("[SCHED] Morning soon - opening halfway");
    routinePreStage(motorLeft, true);
    routinePreStage(motorRight, true);
  }
  if (morningSched.due(t)) {
    Serial.println("[SCHED] Morning time - opening fully");
    routineFinal(motorLeft, true);
    routineFinal(motorRight, true);
  }

  if (nightSched.preDue(t, ROUTINE_LEAD_MIN)) {
    Serial.println("[SCHED] Night soon - closing halfway");
    routinePreStage(motorLeft, false);
    routinePreStage(motorRight, false);
  }
  if (nightSched.due(t)) {
    Serial.println("[SCHED] Night time - closing fully");
    routineFinal(motorLeft, false);
    routineFinal(motorRight, false);
  }
}
