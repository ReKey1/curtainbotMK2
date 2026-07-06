// config.h — Single source of truth for every tunable in the sketch.
// Change values here; no other file should contain magic numbers.
#pragma once
#include <Arduino.h>

// ===== Web server ==========================================================
const uint16_t HTTP_PORT = 80;

// ===== WiFi ================================================================
// How long setup() waits for WiFi before giving up and continuing.
// (Physical buttons keep working without WiFi; the radio retries in the
// background and the onboard LED shows connection state.)
const unsigned long WIFI_TIMEOUT_MS = 20000;

// ===== NTP / timezone ======================================================
// GMT_OFFSET_SEC is the UTC offset in seconds; DAYLIGHT_OFFSET_SEC is the DST
// correction when in effect. Japan (JST, UTC+9) has no daylight saving.
const char* const NTP_SERVER          = "pool.ntp.org";
const long        GMT_OFFSET_SEC      = 9 * 3600;  // Japan Standard Time
const int         DAYLIGHT_OFFSET_SEC = 0;

// ===== Motor tuning ========================================================
const int MOTOR_SPEED   = 180;   // PWM target, 0-255
const int MOTOR_RAMP_MS = 1000;  // ms to ramp from 0 to MOTOR_SPEED

// Physical mapping: motor1 (GPIO 16/17/5) drives the RIGHT curtain, motor2
// (GPIO 18/19/4) the LEFT — see the motorLeft/motorRight aliases in control.h.
// Flip a flag here if a motor runs backwards (open closes it).
const bool MOTOR1_REVERSED = false;  // right curtain
const bool MOTOR2_REVERSED = true;   // left curtain

// Stall detection (how the motors detect the curtain's end of travel).
// Self-tuning: after the ramp, each move learns its own healthy cruise rate —
// the highest encoder delta seen per STALL_CHECK_MS window. That baseline
// ratchets up only, so a belt slipping at the limit (slow but nonzero ticks)
// cannot drag the threshold down with it. A stall is declared when the rate
// drops below STALL_RATIO of the cruise rate — but never below the fixed
// STALL_MIN_TICKS floor — sustained for STALL_TIMEOUT_MS. Re-tunes on every
// move, absorbing day-to-day belt tension changes.
const int   STALL_CHECK_MS   = 30;    // how often to sample the encoder
const int   STALL_MIN_TICKS  = 15;    // threshold floor (also covers moves that start stalled)
const int   STALL_TIMEOUT_MS = 5;     // how long "slow" must persist to confirm
const float STALL_RATIO      = 0.5f;  // stall when rate < ratio * this move's cruise rate

// ===== Position tracking / go-to ===========================================
// A stall this far (in % of travel) from the expected endpoint is treated as
// an obstruction (motor stops) instead of end-of-travel, so a jam can't
// corrupt the position calibration.
const float ENDPOINT_TOLERANCE_PCT = 15.0f;
// Go-to-position moves closer than this many encoder ticks are treated as
// "already there" (avoids twitching, absorbs coast-down overshoot).
const int GOTO_DEADBAND_TICKS = 15;

// ===== Routine staging ======================================================
// Morning/night routines: ROUTINE_LEAD_MIN minutes before the set time the curtains move to
// ROUTINE_MID_PCT, then fully open/close exactly at the set time. The
// pre-stage needs position calibration (one full close + full open);
// uncalibrated motors skip it and just do the full move at the set time.
const int ROUTINE_LEAD_MIN = 5;   // minutes before the set time for the half move
const int ROUTINE_MID_PCT  = 50;  // where the pre-stage parks the curtains (% open)

// ===== Schedule defaults (overridable at runtime via the web UI) ===========
const int  DEFAULT_MORNING_HOUR    = 7;
const int  DEFAULT_MORNING_MIN     = 30;
const bool DEFAULT_MORNING_ENABLED = false;

const int  DEFAULT_NIGHT_HOUR      = 22;
const int  DEFAULT_NIGHT_MIN       = 0;
const bool DEFAULT_NIGHT_ENABLED   = false;
