#include <EEPROM.h>
#include <Streaming.h>

#include <Time.h>
#include <SimpleTimer.h>
#include <SerialCommand.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FiniteStateMachine.h>
#include <SyncLED.h>

#include "Utils.h"
#include "Scheduler.h"
#include "Thermal.h"

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
 stm  scheduletimers  
 
 FSM debug messages
 ------------------
 
 se  state enter
 su  update
 sx  exit
 wl  water level
 hl  high limit
 
 
 
 Pins
 ----
 
 0    Error     -            3
 1    1-Wire    25 (PC2)    12    Brown
 2    Safety    16 (PB2)    11    Red
 3    Pump      14 (PD7)     9    Orange
 4    Heat      13 (PB0)     8    Yellow
 5    Aux       12 (PD6)     6    Green
 6    WL Out    27 (PC4)    A0    White
 7    WL In     23 (PC0)    A1    Orange'
 8    HL In     28 (PC5)    A2    Blue'
 
 x    GND        7         GND    Black
 
      VCC        8                Blue
      AREF      21
 */
 
#define NUM_PINS     9

#define ERR_LED      3
#define ONE_WIRE    12
#define RLY_SAFETY  11
#define RLY_PUMP     9
#define RLY_HEAT     8
#define RLY_AUX      6
#define WL_OUT      A1
#define WL_IN       A0
#define HL_IN       A2

 // ====================================================================

// EEPROM

#define RUN    42
#define TEST   23

struct System
{
  byte magic;
  float sp;
};

System sys;

// Test

SimpleTimer tt;

int tp[] =
{
  ERR_LED, ONE_WIRE, RLY_SAFETY, RLY_PUMP, RLY_HEAT, RLY_AUX, WL_OUT, WL_IN, HL_IN
};

int tpc = 0;

// ====================================================================

// Schedule


ScheduleItem scheduleItems[] =
{
//  Days         Start    End      Period   MinDuty MaxDuty

  // Exceptions
  { Fri        , hr(18) , hr(24) , hr(1)  , mn(20) , hr(1) },

  // Shoulder - 7am to 1pm & 8pm to 10pm
  { Weekdays   , hr(20) , hr(22) , hr(1)  , 0      , 0     },
  { Weekdays   , hr(7)  , hr(13) , hr(1)  , 0      , 0     },
  
  // Peak - 1pm to 8pm Mon-Fri
  { Weekdays   , hr(13) , hr(20) , hr(1)  , 0      , 0     },
  
  // Default (Off Peak)
  { AllWeek    , hr(00) , hr(24) , hr(1)  , mn(5) , hr(1) }
};

/*
ScheduleItem scheduleItems[] =
{
  { AllWeek    , hr(00) , hr(24) , 15     , 5      , 10 }
};
*/

Scheduler sch(scheduleItems, sizeof(scheduleItems) / sizeof(ScheduleItem));

#define soakDuration hr(1)
#define rapidHeatDuration hr(24)
 
// ====================================================================

// Main error

enum Status
{
  Ok, Unknown, TemperatureSensor, WaterLevelSensor, HighLimit
};

Status err = Ok;
Status errDisplay = Ok;

SyncLED el(ERR_LED, LOW, false, 300UL);

// Serial Commands

SerialCommand cmd;

// Thermal Sensor

Thermal th(ONE_WIRE, onTemperatureChanged);

// Water level sensor

struct LevelSensor
{
  int out, in, threshold, value;
};

LevelSensor ls =
{
  WL_OUT, WL_IN, 512, 0
};

// Heater element AC sense / High limit cutout

struct HighLimit_
{
  int in, threshold, value;
};

HighLimit_ hl =
{
  HL_IN, 100, 0
};

// ====================================================================

// Relays

struct Relays
{
  byte safety, pump, heat, aux;
};

Relays relays = 
{
  RLY_SAFETY, RLY_PUMP, RLY_HEAT, RLY_AUX
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

// --------------------------------------------------------------------

// Mode FSM

struct Mode
{
  State error, init, off, autoheat, rapidheat, soak, filter;
};

Mode mode = 
{
  State(enterModeError),
  State(enterModeInit, updateModeInit, exitModeInit),
  State(enterModeOff),
  State(0, updateModeAutoHeat, 0),
  State(enterModeRapidHeat, updateModeRapidHeat, exitModeRapidHeat),
  State(enterModeSoak, updateModeSoak, exitModeSoak),
  State(enterModeFilter, 0, exitModeFilter)
};

Array<Mode, State> _mode(mode);

// --------------------------------------------------------------------

// Pump FSM

enum ePump { pError, pOff, pOn, pHeat };

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

// --------------------------------------------------------------------

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
  fsm.mode.setup(0, _mode.count, _mode.data, sendStates);
  fsm.pump.setup(1, _pump.count, _pump.data, sendStates);
  fsm.aux.setup(2, _aux.count, _aux.data, sendStates);
}

// ====================================================================

// Serial Commands

void setupTestSerialCommands()
{
  cmd.addCommand("rst", softReset);
  cmd.addCommand("tp", onTestPin);
  cmd.addCommand("end", endTest);
}

void setupSerialCommands()
{
  cmd.addCommand("rst", softReset);
  cmd.addCommand("test", beginTest);
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
  cmd.addCommand("stm", sendScheduleTimers);

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
    Serial << " " << (s ? s->index() : 100);
  }
  
  Serial << " " << err;
  
  //Serial << " " << th.triggerState() << "  " << sch.dutyState();

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
         
  Serial << "sns "
  << " " << ls.value
  << " " << hl.value
  << endl;
         
  sch.printTimers();
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
    Serial << "mi " << mi << " numStates " << fsm.mode.numStates() << endl;
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

  sys.sp = sp;  
  writeSystem();

  sendTemperature();
}

void sendSetPoint()
{
  Serial << "sp " << th.setPoint() << endl;
}

// --------------------------------------------------------------------
// schedule

void sendSchedules()
{
  sch.printSchedule();
}

// --------------------------------------------------------------------
// scheduletimers

void sendScheduleTimers()
{
  sch.printTimers();
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

// MISC IO

void setupIO()
{
  pinMode(ls.out, OUTPUT);
  pinMode(ls.in, INPUT);

  pinMode(hl.in, INPUT);
}

void checkWaterLevel()
{
  digitalWrite(ls.out, HIGH);
  delay(20);

  ls.value = analogRead(ls.in);
  digitalWrite(ls.out, LOW);
  
  if (ls.value > ls.threshold)
    return;
    
  if (!err)
  {
    err = WaterLevelSensor;
    mode.error.immediateTransition();
    sendStates();
  }
}

void checkHighLimit()
{
  hl.value = analogRead(hl.in);
  
  if (hl.value < hl.threshold)
    return;
    
  if (!pump.heat.current())
    return;

  if (!err)
  {  
    err = HighLimit;
    mode.error.immediateTransition();
    sendStates();
  }
}

// ====================================================================

// Mode FSM Handlers

void enterModeError()
{
  if (!pump.error.current())
    pump.error.immediateTransition();
    
  if (!aux.off.current())
    aux.off.immediateTransition();
    
  el.setRate(100UL);
  el.blinkPattern((byte)err, 100UL);
}

// --------------------------------------------------------------------

// Init

void enterModeInit()
{
  setupSerialCommands();

  setupRelays();

  th.setup(sys.sp);
  
  el.blinkPattern((byte)1, 300UL);

  if (err)
    mode.error.immediateTransition();
}

void updateModeInit()
{
  if (timeStatus() == timeNotSet)
    return;

  mode.off.transition();
  //mode.autoheat.transition();
  //pump.off.transition();
  //aux.off.transition();
}

void exitModeInit()
{
  el.Off();
}

// --------------------------------------------------------------------

// Off

void enterModeOff()
{
  pump.off.transition();
  aux.off.transition();
  
  el.Off();
}

// --------------------------------------------------------------------

// Filter

void enterModeFilter()
{
  sch.manual(rapidHeatDuration);
  pump.on.transition();
}

void exitModeFilter()
{
  sch.reset();
}

// --------------------------------------------------------------------

// AutoHeat

void updateModeAutoHeat()
{
  ePump p = pOff;
  
  if (sch.dutyState() != dsOver)
  {
    p = (th.triggerState() == tsLow && !aux.on.current())
      ? pHeat : pOn;
  }
  
  if (_pump[p].current())
    return;
  
  _pump[p].transition();
}

// --------------------------------------------------------------------

// RapidHeat

void enterModeRapidHeat()
{
  sch.manual(rapidHeatDuration);
}

void updateModeRapidHeat()
{ 
  if (sch.dutyState() == dsOver ||
      th.triggerState() == tsHigh ||
      aux.on.current())
  {
    mode.autoheat.transition();
    return;
  }
  
  pump.heat.transition();
}

void exitModeRapidHeat()
{
  sch.reset();
}

// --------------------------------------------------------------------

void enterModeSoak()
{
  sch.manual(soakDuration);
}

void updateModeSoak()
{
  if (sch.dutyState() == dsOver)
  {
    mode.autoheat.transition();
    return;
  }
  
  if (th.triggerState() == tsHigh || 
      aux.on.current())
  {
    pump.on.transition();
    return;
  }
  
  pump.heat.transition();
}

void exitModeSoak()
{
  sch.reset();
}

// --------------------------------------------------------------------

// Pump FSM Handlers

void enterPumpError()
{
  aux.off.immediateTransition();

  setPumpRelays(0, 0, 0);
  sch.stopDutyTimer();
}

void enterPumpOff()
{  
  setPumpRelays(1, 0, 0);
  sch.stopDutyTimer();
}

void enterPumpOn()
{
  setPumpRelays(1, 1, 0);
  sch.startDutyTimer();  
}

void enterPumpHeat()
{
  if (aux.on.current())
  {
    pump.on.immediateTransition();
    return;
  }
      
  setPumpRelays(1, 1, 1);
  sch.startDutyTimer();
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
// Test

void onTestPin()
{
  char *arg = cmd.next();

  if (!arg)
  {
    Serial << "huh?" << endl;
    return;
  }

  int p = strtol(arg, NULL, 10);

  if ((p <= 0 && strcmp(arg, "0")) || p >= NUM_PINS)
  {
    sendInvalidArg();
    return;
  }
  
  digitalWrite(tp[tpc], LOW);
  
  tpc = p;
}

void beginTest()
{
  sys.magic = TEST;
  writeSystem();

  softReset();
}

void updateTest(void*)
{
  int p = tp[tpc];
  int v = !digitalRead(p);
  
  Serial << "tp"
  << " " << tpc
  << " " << p 
  << " " << v
  << endl;
  
  digitalWrite(p, v);
}

void endTest()
{
  sys.magic = RUN;
  writeSystem();
  
  softReset();
}

// ====================================================================
// Setup

void softReset()
{
  setPumpRelays(0, 0, 0);
  setAuxRelay(0);
  
  asm volatile ("  jmp 0");
}

void readSystem()
{
  int bytes = EEPROM_readAnything(0, sys);
  
  Serial << "sys"
  << " " << bytes
  << " " << sys.magic
  << " " << sys.sp
  << endl;
  
  if (sys.magic == 23 || sys.magic == 42)
    return;

  Serial << "no magic, writing defaults to eeprom" << endl;
  
  sys.magic = 42;
  sys.sp = 20.0f;
  
  writeSystem();
}

void writeSystem()
{
  int bytes = EEPROM_writeAnything(0, sys);
}

void testSetup()
{
  setupTestSerialCommands();
  
  for (int i = 0; i < NUM_PINS; ++i)
    pinMode(tp[i], OUTPUT);
  
  tt.setInterval(2000, updateTest);
}

void setup(void)
{
  Serial.begin(9600);
  
  readSystem();
  
  if (sys.magic == TEST)
    return testSetup();
  
  Serial << "ok, give me bubbles!" << endl;
  
  setupIO();
  
  setupFSMs();

  setSyncProvider(requestSync);

  mode.init.transition();
  
  Serial << "ts " << timeStatus() << endl;

  Serial << "SRAM " << freeRam() << " bytes" << endl;  
}

// Main

void testLoop()
{
    tt.run();

    cmd.readSerial();
}

void loop(void)
{
  if (sys.magic == TEST)
    return testLoop();
  
  checkWaterLevel();
  checkHighLimit();
  
  el.update();
  
  th.update();
  
  sch.update();

  for (int i = 0; i < _fsm.count; ++i)
    _fsm[i].update();

  cmd.readSerial();
}
