// commands.h — Command registry: every remote action the bot supports.
//
// To add a new action:
//   1. Write a small handler:  bool cmdMyThing(JsonObjectConst args) { ...; return true; }
//   2. Add one row to COMMANDS[]: { "my_thing", cmdMyThing }
// Nothing else needs to change — the web UI can then POST {"cmd":"my_thing", ...}.
#pragma once
#include <ArduinoJson.h>
#include "control.h"
#include "scheduler.h"
#include "calibration.h"

typedef bool (*CommandFn)(JsonObjectConst args);

struct Command {
  const char* name;  // matches the "cmd" field of POST /command
  CommandFn   fn;    // returns false if the args were invalid
};

// ---- Curtain movement -------------------------------------------------------
bool cmdOpenLeft(JsonObjectConst)   { motorLeft.moveOpen();    return true; }
bool cmdCloseLeft(JsonObjectConst)  { motorLeft.moveClosed();  return true; }
bool cmdStopLeft(JsonObjectConst)   { motorLeft.stop();        return true; }
bool cmdOpenRight(JsonObjectConst)  { motorRight.moveOpen();   return true; }
bool cmdCloseRight(JsonObjectConst) { motorRight.moveClosed(); return true; }
bool cmdStopRight(JsonObjectConst)  { motorRight.stop();       return true; }
bool cmdOpenBoth(JsonObjectConst)   { motorLeft.moveOpen();    motorRight.moveOpen();   return true; }
bool cmdCloseBoth(JsonObjectConst)  { motorLeft.moveClosed();  motorRight.moveClosed(); return true; }
bool cmdStopBoth(JsonObjectConst)   { motorLeft.stop();        motorRight.stop();       return true; }

// ---- Go to position ---------------------------------------------------------
// args: pos (0 = closed .. 100 = open). Fails if the motor is not calibrated
// (needs one full close + full open first).
bool gotoPercent(Motor& m, JsonObjectConst args) {
  int pos = args["pos"] | -1;
  if (pos < 0 || pos > 100) return false;
  return m.moveToPercent((float)pos);
}

bool cmdGotoLeft(JsonObjectConst args)  { return gotoPercent(motorLeft, args); }
bool cmdGotoRight(JsonObjectConst args) { return gotoPercent(motorRight, args); }
bool cmdGotoBoth(JsonObjectConst args) {
  bool l = gotoPercent(motorLeft, args);
  bool r = gotoPercent(motorRight, args);
  return l && r;
}

// ---- Schedules -------------------------------------------------------------
// args: hour (0-23), min (0-59), enabled (bool); missing fields keep their
// current value (ArduinoJson's `|` default operator).
bool setSchedule(CurtainSchedule& sched, JsonObjectConst args) {
  int  h  = args["hour"]    | sched.hour;
  int  m  = args["min"]     | sched.min;
  bool en = args["enabled"] | sched.enabled;
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  sched.hour    = h;
  sched.min     = m;
  sched.enabled = en;
  return true;
}

bool cmdSetMorning(JsonObjectConst args) { return setSchedule(morningSched, args); }
bool cmdSetNight(JsonObjectConst args)   { return setSchedule(nightSched, args); }

// ---- Maintenance ------------------------------------------------------------
bool cmdRecalibrate(JsonObjectConst) {
  calibrationClear();
  return true;
}

// ---- Registry --------------------------------------------------------------
const Command COMMANDS[] = {
  { "open_left",   cmdOpenLeft   },
  { "close_left",  cmdCloseLeft  },
  { "stop_left",   cmdStopLeft   },
  { "open_right",  cmdOpenRight  },
  { "close_right", cmdCloseRight },
  { "stop_right",  cmdStopRight  },
  { "open_both",   cmdOpenBoth   },
  { "close_both",  cmdCloseBoth  },
  { "stop_both",   cmdStopBoth   },
  { "goto_left",   cmdGotoLeft   },
  { "goto_right",  cmdGotoRight  },
  { "goto_both",   cmdGotoBoth   },
  { "set_morning", cmdSetMorning },
  { "set_night",   cmdSetNight   },
  { "recalibrate", cmdRecalibrate },
};

enum class CmdResult { OK, UNKNOWN, FAILED };

CmdResult dispatchCommand(const char* name, JsonObjectConst args) {
  for (const Command& c : COMMANDS) {
    if (strcmp(c.name, name) == 0) {
      return c.fn(args) ? CmdResult::OK : CmdResult::FAILED;
    }
  }
  return CmdResult::UNKNOWN;
}
