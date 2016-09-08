#include <EEPROM.h>
#include <Time.h>
#include <SimpleTimer.h>
#include <Wire.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS3232RTC.h>
#include <FiniteStateMachine.h>
#include <SyncLED.h>
#include <TimerOne.h>

#include "Utils.h"
#include "Scheduler.h"
#include "Thermal.h"
#include "Button.h"
#include "ControlPad.h"

// ====================================================================

//#define NOTEMP
//#define NOWATER
//#define NOHIGHLIMIT

/*

 Pins
 ----

 0    Error     -            3
 1    1-Wire    25 (PC2)    12    Brown
 2    Safety    16 (PB2)    11    Red
 3    Pump      14 (PD7)     9    Orange
 4    Heat      13 (PB0)     8    Yellow
 5    Aux       12 (PD6)     6    Green
 6    WL In     23 (PC0)    A0    White
 7    WL Out    27 (PC4)    A1    Blue'
 8    HL In     28 (PC5)    A2    Orange'

 x    GND        7         GND    Black

 VCC        8                Blue
 AREF      21
 */

#define NUM_PINS     9

/*
#define ERR_LED      3

 #define RLY_SAFETY  11
 #define RLY_PUMP     9
 #define RLY_HEAT     8
 #define RLY_AUX      6

 #define WL_OUT      A1

 #define WL_IN       A0
 #define HL_IN       A2

 #define ONE_WIRE    12
 */

#define ERR_LED      0

#define RLY_SAFETY   4
#define RLY_PUMP     3
#define RLY_HEAT     2
#define RLY_AUX      1

#define PAD_PZ       5

#define PAD_LEFT     9
#define PAD_RIGHT    8
#define PAD_UP       6
#define PAD_DOWN     7

#define SHR_LATCH   10
#define SHR_DATA    11 // MOSI
// MISO
#define SHR_CLK     13 // SCK

#define WL_OUT      A0
#define WL_IN       A1
#define HL_IN       A2
#define ONE_WIRE    A3 //A3
#define I2C_SDA     A4 // RTC
#define I2C_SCL     A5 // RTC


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

// Pad timer

SimpleTimer pt;

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
  {   Fri        , hr(18) , hr(24) , hr(1)  , mn(20) , hr(1)   },

  // Shoulder - 7am to 1pm & 8pm to 10pm
  {   Weekdays   , hr(20) , hr(22) , hr(1)  , 0      , 0       },
  {   Weekdays   , hr(7)  , hr(13) , hr(1)  , 0      , 0       },

  // Peak - 1pm to 8pm Mon-Fri
  {   Weekdays   , hr(13) , hr(20) , hr(1)  , 0      , 0       },

  // Default (Off Peak)
  {   AllWeek    , hr(00) , hr(24) , hr(1)  , mn(5) , hr(1)    },
};

Scheduler sch(scheduleItems, sizeof(scheduleItems) / sizeof(ScheduleItem));

#define soakDuration hr(1)
#define rapidHeatDuration hr(24)

// ====================================================================

// Main error

enum Status
{
  Ok, Unknown, Clock, TemperatureSensor, WaterLevelSensor, HighLimit
};

Status err = Ok;
Status errDisplay = Ok;

SyncLED el(ERR_LED, LOW, false, 300UL);

char *errStr[] =
{
  "  ", "ERR", "RTC", "TPS ", "H20", "CUR"
};

// Thermal Sensor

Thermal th(ONE_WIRE, onTemperatureUpdated);

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

Buttons buttons =
{
  Button(PAD_LEFT),
  Button(PAD_RIGHT),
  Button(PAD_UP),
  Button(PAD_DOWN)
};

Array<Buttons, Button> _buttons(buttons);

ControlPad pad(SHR_CLK, SHR_LATCH, SHR_DATA, _buttons);

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
  FSM mode, pump, aux, menu;
};

FSMS fsm;

Array<FSMS, FSM> _fsm(fsm);

// --------------------------------------------------------------------

// Mode FSM

struct Mode
{
  State error, init, off, autoheat, soak, rapidheat;
};

Mode mode =
{
  State(enterModeError),
  State(enterModeInit, updateModeInit, exitModeInit),
  State(enterModeOff),
  State(enterModeAutoHeat, updateModeAutoHeat, 0),
  State(enterModeSoak, updateModeSoak, exitModeSoak),
  State(enterModeRapidHeat, updateModeRapidHeat, exitModeRapidHeat)
};

Array<Mode, State> _mode(mode);

char *modeStr[] =
{
  "ERR", "INI", "OFF", "AUT", "ON ", "RPD"
};

// --------------------------------------------------------------------

// Pump FSM

enum ePump
{
  pError, pOff, pOn, pHeat
};

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

// --------------------------------------------------------------------

// Menu FSM

struct Menu
{
  State error, mode, setpoint, timer, blower, footwell, steps, head, path, softreset;
};

Menu menu =
{
  State(enterMenuError, updateMenuError, 0),
  State(enterMenuMode, updateMenuMode, 0),
  State(enterMenuSetpoint, updateMenuSetpoint, 0),
  State(enterMenuTimer, updateMenuTimer, 0),
  State(enterMenuBlower, updateMenuBlower, 0),
  State(enterMenuFootwell, updateMenuFootwell, 0),
  State(enterMenuSteps, updateMenuSteps, 0),
  State(enterMenuHead, updateMenuHead, 0),
  State(enterMenuPath, updateMenuPath, 0),
  State(enterMenuReset, updateMenuReset, 0)
};

Array<Menu, State> _menu(menu);

// --------------------------------------------------------------------

// Setup

void setupFSMs()
{
  fsm.mode.setup(0, _mode.count, _mode.data);
  fsm.pump.setup(1, _pump.count, _pump.data);
  fsm.aux.setup(2, _aux.count, _aux.data);
  fsm.menu.setup(3, _menu.count, _menu.data);

  // mode links

  mode.off.link(NULL, &mode.autoheat);
  mode.autoheat.link(&mode.off, &mode.soak);
  mode.soak.link(&mode.autoheat, &mode.rapidheat);
  mode.rapidheat.link(&mode.soak, NULL);

  // menu links

  for (int i = 1; i < _menu.count; ++i)
  {
    int p = i - 1;
    if (p == 0) p = _menu.count - 1;

    int n = i + 1;
    if (n == _menu.count) n = 1;

    _menu[i].link(&_menu[p], &_menu[n]);
  }
}

// ====================================================================


// --------------------------------------------------------------------
// error

bool checkError(State &errorState)
{
  if (errorState.current())
  {
    //showError();
    return true;
  }

  return false;
}

// ====================================================================

// Temperature Sensor

void onTemperatureUpdated()
{
#ifdef NOTEMP
  th.setTemperature(37.6);
#else
  if (th.error())
    err = TemperatureSensor;
#endif
}

// ====================================================================

// Relays

void setupRelays()
{
  for (byte i = 0; i < _relays.count; ++i)
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

  pad.d.safety = (safety == 0);
  pad.d.pump = (pump == 0);
  pad.d.heat = (heat == 0);
}

void setAuxRelay(int aux)
{
  digitalWrite(relays.aux, aux);

  pad.d.aux = (aux == 0);
}

// ====================================================================

// MISC IO

void setupIO()
{
  pinMode(ls.out, OUTPUT);
  pinMode(ls.in, INPUT);
  digitalWrite(ls.out, HIGH);

  pinMode(hl.in, INPUT);
}

void checkWaterLevel()
{
#ifdef NOWATER
  return;
#else
  ls.value = analogRead(ls.in);

  if (ls.value > ls.threshold)
    return;

  if (!err)
  {
    err = WaterLevelSensor;
    mode.error.immediateTransition();
    //sendStates();
  }
#endif
}

void checkHighLimit()
{
#ifdef NOHIGHLIMIT
  return;
#else
  hl.value = analogRead(hl.in);

  if (hl.value < hl.threshold)
    return;

  if (!pump.heat.current())
    return;

  if (!err)
  {
    err = HighLimit;
    mode.error.immediateTransition();
  }
#endif
}

// ====================================================================

// Mode FSM Handlers

void enterModeError()
{
  if (!pump.error.current())
    pump.error.immediateTransition();

  if (!aux.off.current())
    aux.off.immediateTransition();

  menu.error.transition();

  el.setRate(100UL);
  el.blinkPattern((byte)err, 100UL);
}

// --------------------------------------------------------------------

// Init

void enterModeInit()
{
  menu.mode.transition();

  setupRelays();

  if (!th.setup(sys.sp))
  {
#ifdef NOTEMP
#else
    //err = TemperatureSensor;
#endif
  }

  el.blinkPattern((byte)1, 300UL);

  if (err)
    mode.error.immediateTransition();
}

void updateModeInit()
{
  if (timeStatus() == timeNotSet)
    return;

  mode.off.transition();
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
  menu.mode.transition();

  el.Off();
}

// --------------------------------------------------------------------

// AutoHeat

void enterModeAutoHeat()
{
  menu.mode.transition();
}

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
  //sendRelays();
}

void enterAuxOn()
{
  if (pump.heat.current())
    pump.on.immediateTransition();

  setAuxRelay(1);
  //sendRelays();
}

// ====================================================================

// Menu FSM Handlers

void menuTimeout(void*)
{
  if (menu.timer.current() && mode.soak.current())
    return;

  menu.mode.transition();
}

int menuTimeoutId = -1;

void navigateMenu()
{
  if (pad.buttonChanged)
  {
    if (pt.isEnabled(menuTimeoutId))
      pt.restartTimer(menuTimeoutId);
    else
      menuTimeoutId = pt.setTimeout(15000, menuTimeout);

    State *menu = fsm.menu.currentState();
    State *nextMenu = NULL;

    if (buttons.left.down())
      nextMenu = menu->previous();
    else if (buttons.right.down())
      nextMenu = menu->next();

    if (nextMenu)
      nextMenu->transition();
  }
}

void enterMenuError()
{
  pad.seg.encodeString(errStr[(byte)err]);
}

bool errorLeft = false;
bool errorRight = false;

void updateMenuError()
{
  if (buttons.incr.down())
    softReset();
}

void enterMenuMode()
{
}

void updateMenuMode()
{
  byte modeIndex = fsm.mode.currentState()->index();
  pad.seg.encodeString(modeStr[modeIndex]);

  State *mode = fsm.mode.currentState();
  State *nextMode = NULL;

  if (buttons.incr.down())
    nextMode = mode->next();
  else if (buttons.decr.down())
    nextMode = mode->previous();

  if (nextMode)
    nextMode->transition();
}

bool editSetpoint = false;

void enterMenuSetpoint()
{
  editSetpoint = false;
}

void updateMenuSetpoint()
{
  if (!editSetpoint)
  {
    pad.seg.setFloat(th.temperature());

    editSetpoint = buttons.incr.down() || buttons.decr.down();
  }
  else
  {
    pad.seg.setFloat(th.setPoint());

    bool edited = false;

    if (buttons.decr.down())
    {
      th.setSetPoint(th.setPoint() - 0.1);
      edited = true;
    }

    if (buttons.incr.down())
    {
      th.setSetPoint(th.setPoint() + 0.1);
      edited = true;
    }

    if (edited)
    {
      sys.sp = th.setPoint();
      writeSystem();
    }
  }
}

void enterMenuTimer()
{
}

bool menuTimerBlink = false;

void onMenuTimerBlink(void*)
{
  menuTimerBlink = !menuTimerBlink;
}

void updateMenuTimer()
{
  if (!mode.soak.current())
  {
    pad.seg.encodeString("---");
  }
  else
  {
    pad.seg.setTimer(sch.remaining(), menuTimerBlink);

    if (buttons.incr.down())
    {
      sch.manual(soakDuration);
    }
  }
}

void enterMenuBlower()
{
  pad.seg.encodeString("BL. ");
}

void updateMenuBlower()
{
  pad.seg.setSwitch(aux.on.current());

  if (buttons.incr.down() && aux.off.current())
    aux.on.transition();

  if (buttons.decr.down() && aux.on.current())
    aux.off.transition();
}

void enterMenuFootwell()
{
  pad.seg.encodeString("FT. ");
}

void updateMenuFootwell()
{
  pad.seg.setSwitch(pad.d.footwell);

  if (buttons.incr.down() && !pad.d.footwell)
    pad.d.footwell = HIGH;

  if (buttons.decr.down() && pad.d.footwell)
    pad.d.footwell = LOW;
}

void enterMenuSteps()
{
  pad.seg.encodeString("ST. ");
}

void updateMenuSteps()
{
  pad.seg.setSwitch(pad.d.steps);

  if (buttons.incr.down() && !pad.d.steps)
    pad.d.steps = HIGH;

  if (buttons.decr.down() && pad.d.steps)
    pad.d.steps = LOW;
}

void enterMenuHead()
{
  pad.seg.encodeString("HD. ");
}

void updateMenuHead()
{
  pad.seg.setSwitch(pad.d.head);

  if (buttons.incr.down() && !pad.d.head)
    pad.d.head = HIGH;

  if (buttons.decr.down() && pad.d.head)
    pad.d.head = LOW;
}

void enterMenuPath()
{
  pad.seg.encodeString("PT. ");
}

void updateMenuPath()
{
  pad.seg.setSwitch(pad.d.path);

  if (buttons.incr.down() && !pad.d.path)
    pad.d.path = HIGH;

  if (buttons.decr.down() && pad.d.path)
    pad.d.path = LOW;
}

void enterMenuReset()
{
  pad.seg.encodeString("RST");
}

void updateMenuReset()
{
  if (buttons.incr.down())
    softReset();
}


// ====================================================================

// Setup

void softReset()
{
  setPumpRelays(0, 0, 0);
  setAuxRelay(0);

  Timer1.detachInterrupt();

  asm volatile ("  jmp 0");
}

void readSystem()
{
  int bytes = EEPROM_readAnything(0, sys);

  if (sys.magic == 23 || sys.magic == 42)
    return;

  sys.magic = 42;
  sys.sp = 20.0f;

  writeSystem();
}

void writeSystem()
{
  EEPROM_writeAnything(0, sys);
}

void setup(void)
{
  readSystem();

  RTC.set33kHzOutput(false);
  RTC.clearAlarmFlag(3);

  setupIO();

  setupFSMs();

  setSyncProvider(RTC.get);

  if (!RTC.available())
    err = Clock;

  pad.setup();
  //pt.setInterval(5, updatePad);
  pt.setInterval(500, onMenuTimerBlink);

  Timer1.initialize(5000);
  Timer1.attachInterrupt(padISR);

  mode.init.transition();
}

// ====================================================================

// Main

//void updatePad(void*)
void padISR()
{
  pad.update();
}

void loop(void)
{
  checkWaterLevel();
  checkHighLimit();

  th.update();

  el.update();

  sch.update();

  pt.run();

  pad.updateButtons();

  navigateMenu();

  for (byte i = 0; i < _fsm.count; ++i)
    _fsm[i].update();

  pad.clearButtonEvents();
}
