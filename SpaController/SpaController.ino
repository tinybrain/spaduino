#include <Streaming.h>

#include <Time.h>
#include <SerialCommand.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FiniteStateMachine.h>

#include "Utils.h"
#include "Thermal.h"
#include "Scheduler.h"

// ====================================================================

/*

 Commands
 --------
 
 hi   hello
 ok   ack
 unk  unknown
 err  error
 nr   not ready
 inv  invalid arg
 st   states
 syn  sync
 ti   time
 tm   temp
 rly  relays
 md   mode
 a    aux
 sp   setpoint
 sc   schedules
 
 FSM debug messages
 ------------------
 
 se  state enter
 su  update
 sx  exit
 
 */
 
// ====================================================================

// Main error

enum Status
{
  Ok, Unknown, TemperatureSensor, WaterLevelSensor, HighLimit
};

Status err = Ok;

// Serial Commands

SerialCommand cmd;

// Temperature

Thermal th(12, onTemperatureChanged);

// Water level sensor


// Heater element AC sense / High limit cutout


// Relays

struct Relays
{
  byte safety, pump, heat, aux;
};

Relays relays = 
{
  8, 9, 10, 11
};

Array<Relays, byte> _relays(relays);

// ====================================================================

// FSM list

struct FSMS
{
  FSM mode, pump, aux;
};

FSMS fsm;

Array<FSMS, FSM> _fsm(fsm);

// Mode FSM

struct Mode
{
  State error, init, off, filter, autoheat, rapidheat, soak; 
};

Mode mode = 
{
  State(enterModeError),
  State(enterModeInit, updateModeInit),
  State(enterModeOff),
  State(enterModeFilter),
  State(enterModeAutoHeat, updateModeAutoHeat),
  State(enterModeRapidHeat),
  State(enterModeSoak)
};

Array<Mode, State> _mode(mode);

void onSetMode(eMode mode)
{
  _mode[mode].transition();
}

// Pump FSM

struct Pump
{
  State error, off, on, heat;
};

Pump pump =
{
  State(enterPumpError),
  State(enterPumpOff),
  State(enterPumpOn),
  State(enterPumpHeat)
};

Array<Pump, State> _pump(pump);

// Aux FSM

struct Aux
{
  State off, on;
};

Aux aux =
{
  State(enterAuxOff),
  State(enterAuxOn)
};

Array<Aux, State> _aux(aux);

// Setup

void setupFSMs()
{
  fsm.mode.setup(0, _mode.count, _mode.data);
  fsm.pump.setup(1, _pump.count, _pump.data);
  fsm.aux.setup(2, _aux.count, _aux.data);
}

// ====================================================================

// Schedule

/*
ScheduleItem scheduleItems[] =
{
//  Days         Start    End      Period   PrefDuty MaxDuty
  { Fri | Sat  , hr(18) , hr(24) , hr(1)  , mn(20) , hr(1) },
  { Sat | Sun  , hr(00) , hr(02) , hr(1)  , mn(20) , hr(1) },
  { AllWeek    , hr(00) , hr(8)  , hr(4)  , mn(20) , hr(4) },
  { AllWeek    , hr(00) , hr(24) , hr(24) , 0      , 0 }
};
*/

ScheduleItem scheduleItems[] =
{
  { AllWeek    , hr(00) , hr(24) , 15     , 5      , 10 }
};

Scheduler sch(scheduleItems, sizeof(scheduleItems) / sizeof(ScheduleItem));


// ====================================================================

// Serial Commands

void setupSerialCommands()
{
  cmd.addCommand("hi", sendAck);
  cmd.addCommand("err", sendError);
  cmd.addCommand("st", sendStates);
  cmd.addCommand("ti", onTime);
  cmd.addCommand("tmp", sendTemperature);
  cmd.addCommand("rly", sendRelays);
  cmd.addCommand("md", onMode);
  cmd.addCommand("a", onAux);
  cmd.addCommand("sp", onSetPoint);
  cmd.addCommand("sc", sendSchedules);

  cmd.setDefaultHandler(onUnknown);
}

// --------------------------------------------------------------------
// ack

void sendAck()
{
  Serial << "ok " << endl;
}

// --------------------------------------------------------------------
// unknown

void onUnknown(const char *cmd)
{
  Serial << "unk" << endl;
}

// --------------------------------------------------------------------
// not ready

void sendNotReady()
{
  Serial << "nr" << endl;
}

// --------------------------------------------------------------------
// invalid

void sendInvalidArg()
{
  Serial << "inv " << cmd.bufferPtr() << endl;
}

// --------------------------------------------------------------------
// error

bool checkError(State &errorState)
{
  if (errorState.current())
  {
    sendError();
    return true;
  }

  return false;
}

void sendError()
{
  Serial << "err " << " " << err << endl;
}

// --------------------------------------------------------------------
// states

void sendStates()
{
  Serial << "st";

  for (int i = 0; i < _fsm.count; ++i)
  {
    FSM &m = _fsm[i];
    State *s = m.currentState();
    Serial << " " << (s ? s->index() : -1);
  }
  
  Serial << " " << th.triggerState() << "  " << sch.dutyState();

  Serial << endl;
}

// --------------------------------------------------------------------
// sync

time_t requestSync()
{
  Serial << "syn" << endl;
  return 0;
}

// --------------------------------------------------------------------
// time

void onTime()
{
  time_t pctime = 0;
  char *arg = cmd.next();

  if (!arg)
  {
    sendTime();
    return;
  }

  pctime = strtol(arg, NULL, 10);

  if (!pctime)
  {
    sendInvalidArg();
    return;
  }

  pctime += 10 * SECS_PER_HOUR;
  
  setTime(pctime);

  sendTime();
}

// --------------------------------------------------------------------
// time

void sendTime()
{
  Serial << "ti " << now() << " " << timeStatus() << endl;
}

// --------------------------------------------------------------------
// temp

void sendTemperature()
{
  Serial << "tmp " << th.temperature() << " " << th.setPoint()
         << " " << _FLOAT(th.rate(), 4) << " " << th.triggerState() << endl;
}

// --------------------------------------------------------------------
// relays

void sendRelays()
{
  Serial << "rly";

  for (int i = 0; i < _relays.count; ++i)
    Serial << " " << digitalRead(_relays[i]);

  Serial << endl;
}

// --------------------------------------------------------------------
// mode

void onMode()
{
  if (checkError(mode.error)) return;

  if (!fsm.mode.currentState()) { 
    sendNotReady(); 
    return; 
  }

  char *arg = cmd.next();

  if (!arg)
  {
    sendMode();
    return;
  }

  int mi = strtol(arg, NULL, 10);

  if ((mi <= 0 && strcmp(arg, "0")) || mi >= fsm.mode.numStates())
  {
    sendInvalidArg();
    return;
  }

  State *s = fsm.mode.states();
  s[mi].transition();
}

void sendMode()
{
  Serial << "md " << fsm.mode.currentState()->index() << endl;
}

// --------------------------------------------------------------------
// aux

void onAux()
{
  if (checkError(mode.error)) return;

  if (!fsm.aux.currentState()) { 
    sendNotReady(); 
    return; 
  }

  toggleAux();
}

// --------------------------------------------------------------------
// setpoint

void onSetPoint()
{
  if (checkError(mode.error)) return;

  char *arg = cmd.next();

  if (!arg)
  {
    sendSetPoint();
    return;
  }

  float sp = (float)atof(arg);

  if (sp == 0.0f && arg[0] != '0')
  {
    sendInvalidArg();
    return;
  }
  
  th.setSetPoint(sp);

  sendSetPoint();
}

void sendSetPoint()
{
  Serial << "sp " << th.setPoint() << endl;
}

// --------------------------------------------------------------------
// schedule

void sendSchedules()
{
  sch.print();
}

// ====================================================================

// Temperature Sensor

void onTemperatureChanged()
{
  if (th.temperature() < -273.15)
    err = TemperatureSensor;

  sendTemperature();
}

// ====================================================================

// Relays

void setupRelays()
{
  for (int i = 0; i < _relays.count; ++i)
  {
    pinMode(_relays[i], OUTPUT);
    digitalWrite(_relays[i], LOW);
  }
}

void setPumpRelays(int safety, int pump, int heat)
{
  digitalWrite(relays.safety, safety);
  digitalWrite(relays.pump, pump);
  digitalWrite(relays.heat, heat);

  sendRelays();
}

void setAuxRelay(int aux)
{
  digitalWrite(relays.aux, aux);

  sendRelays();
}

// ====================================================================

// Mode FSM Handlers

void enterModeError()
{
  if (!pump.error.current())
    pump.error.immediateTransition();
}

void enterModeInit()
{
  setupSerialCommands();

  setupRelays();

  th.setup();

  if (err)
    mode.error.immediateTransition();
}

void updateModeInit()
{
  if (timeStatus() == timeNotSet)
    return;

  mode.autoheat.transition();
}

void enterModeOff()
{
  pump.off.transition();
  aux.off.transition();
}

void enterModeFilter()
{
  pump.on.transition();
}

void enterModeAutoHeat()
{
  pump.off.transition();
  aux.off.transition();
}

// ds under met over
// ts low high

ePump plu[2][3] =
{
  { pHeat, pHeat, pOff },
  { pOn, pOn, pOff }
};

void updateModeAutoHeat()
{
  ePump p = plu[th.triggerState()][sch.dutyState()];
  State &ps = _pump[p];
  
  ps.transition();
}

void enterModeRapidHeat()
{
  aux.off.transition();
  pump.heat.transition();
}

void enterModeSoak()
{
}

// --------------------------------------------------------------------

// Pump FSM Handlers

void enterPumpError()
{
  aux.off.immediateTransition();

  setPumpRelays(0, 0, 0);
}

void enterPumpOff()
{
  sch.stopDutyTimer();
  
  setPumpRelays(1, 0, 0);
}

void enterPumpOn()
{
  sch.startDutyTimer();
  
  setPumpRelays(1, 1, 0);
}

void enterPumpHeat()
{
  sch.startDutyTimer();
  
  setPumpRelays(1, 1, 1);
}

void updatePumpHeat()
{
  if (!err)
    return;

  pump.error.immediateTransition();
}

// --------------------------------------------------------------------

// Aux FSM Handlers

void toggleAux()
{
  if (aux.on.current())
    aux.off.transition();
  else
    aux.on.transition();
}

void enterAuxOff()
{
  setAuxRelay(0);
  sendRelays();
}

void enterAuxOn()
{
  if (pump.heat.current())
    pump.on.immediateTransition();

  setAuxRelay(1);
  sendRelays();
}

// ====================================================================
// Setup

void setup(void)
{
  Serial.begin(9600);

  Serial << "ok, give me bubbles!" << endl;
  
  setupFSMs();

  setSyncProvider(requestSync);

  mode.init.transition();
  
  Serial << "ts " << timeStatus() << endl;

  Serial << "SRAM " << freeRam() << " bytes" << endl;  
}

// Main

void loop(void)
{
  cmd.readSerial();

  th.update();
  
  if (timeStatus() != timeNotSet)
    sch.update();
  
  for (int i = 0; i < _fsm.count; ++i)
    _fsm[i].update();
}
