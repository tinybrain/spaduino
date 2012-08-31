#include <Time.h>
#include <SimpleTimer.h>
#include <Streaming.h>
#include <SerialCommand.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FiniteStateMachine.h>

#include <avr/pgmspace.h>

// --------------------------------------------------------------------

/*

Commands
--------

h  hello
a  ack
u  unknown
e  error
n  not ready
i  invalid arg
s  states
t  time
c  temp
r  relays
m  mode
a  aux

FSM debug messages
------------------

se  state enter
su  update
sx  exit

*/

// --------------------------------------------------------------------

// Main error

enum Status { Ok, Unknown, TemperatureSensor, WaterLevelSensor, HighLimit };
Status err = Ok;

// Serial Commands

SerialCommand cmd;

// Temperature

float temperature = 0.0;

OneWire tsBus(12);
DallasTemperature ts(&tsBus);
DeviceAddress tsAddress;

#define tsResolution 12
#define tsInterval (750 / 1 << (12 - tsResolution))

SimpleTimer tsTimer;

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
  State(enterModeAutoHeat),
  State(enterModeRapidHeat),
  State(enterModeSoak)
};

Array<Mode, State> _mode(mode);

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

// --------------------------------------------------------------------

// Serial Commands

void setupSerialCommands()
{
  cmd.addCommand("h", sendAck);
  cmd.addCommand("e", sendError);
  cmd.addCommand("s", sendStates);
  cmd.addCommand("t", onTime);
  cmd.addCommand("c", sendTemperature);
  cmd.addCommand("r", sendRelays);
  cmd.addCommand("m", onMode);
  cmd.addCommand("a", onAux);

  cmd.setDefaultHandler(onUnknown);
}

// Receive

void onUnknown(const char *cmd)
{
  Serial << "u" << endl;
}

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
  
  setTime(pctime);
  
  sendTime();
}

void onMode()
{
  if (checkError(mode.error)) return;
  
  if (!fsm.mode.currentState()) { sendNotReady(); return; }
  
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

void onAux()
{
  if (checkError(mode.error)) return;
  
  if (!fsm.aux.currentState()) { sendNotReady(); return; }

  toggleAux();
}

// Send

bool checkError(State &errorState)
{
  if (errorState.current())
  {
    sendError();
    return true;
  }
    
  return false;
}

void sendAck()
{
  Serial << "a " << endl;
}

void sendInvalidArg()
{
  Serial << "i " << endl;
}

void sendError()
{
  Serial << "e " << " " << err << endl;
}

void sendStates()
{
  Serial << "s";
  
  for (int i = 0; i < _fsm.count; ++i)
  {
    FSM &m = _fsm[i];
    State *s = m.currentState();
    Serial << " " << m.index() << ":" << (s ? s->index() : -1);
  }

  Serial << endl;
}

void sendNotReady()
{
  Serial << "n" << endl;
}

void sendTime()
{
  Serial << "t " << now() << " " << timeStatus() << endl;
}

void sendTemperature()
{
  Serial << "c " << _FLOAT(temperature, 3) << endl;
}

void sendRelays()
{
  Serial << "r";
  
  for (int i = 0; i < _relays.count; ++i)
    Serial << " " << digitalRead(_relays[i]);
  
  Serial << endl;
}

void sendMode()
{
  Serial << "m " << fsm.mode.currentState()->index() << endl;
}

// --------------------------------------------------------------------

// Clock

time_t requestSync()
{
  Serial << "y" << endl;
}

// --------------------------------------------------------------------

// Temperature

void setupTemperatureSensor()
{
  ts.begin();
  
  if (!ts.getAddress(tsAddress, 0))
  {
    err = TemperatureSensor;
    return;
  }

  ts.setResolution(tsAddress, tsResolution);
  ts.setWaitForConversion(false);
  ts.requestTemperatures();
  
  tsTimer.setInterval(tsInterval, serviceTemperatureSensor);
}

void serviceTemperatureSensor()
{
  float newTemperature = ts.getTempCByIndex(0);
  ts.requestTemperatures();
  
  if (temperature == newTemperature)
    return;
    
  temperature = newTemperature;
  sendTemperature();
}

// --------------------------------------------------------------------

// IO

void setupIO()
{
  for (int i = 0; i < _relays.count; ++i)
  {
    int r = _relays[i];
    pinMode(r, OUTPUT);
    digitalWrite(r, LOW);
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

// --------------------------------------------------------------------

// Mode FSM Handlers

void enterModeError()
{
  if (!pump.error.current())
    pump.error.immediateTransition();
}

void enterModeInit()
{
  setupSerialCommands();
  
  setupIO();
  
  setupTemperatureSensor();
  
  if (err)
    mode.error.immediateTransition();
}

void updateModeInit()
{
  if (timeStatus() == timeNotSet)
    return;
  
  mode.off.transition();  
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
  Serial << "unimplemented" << endl;
  mode.off.transition();
}

void enterModeRapidHeat()
{
  aux.off.transition();
  pump.heat.transition();
}

void enterModeSoak()
{
  Serial << "unimplemented" << endl;
  mode.off.transition();
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
  setPumpRelays(1, 0, 0);
}

void enterPumpOn()
{
  setPumpRelays(1, 1, 0);
}

void enterPumpHeat()
{
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

// --------------------------------------------------------------------

// Setup

void setup(void)
{
  Serial.begin(9600);

  Serial << "ok, give me bubbles!" << endl;
  
  setupFSMs();

  setSyncProvider(requestSync);

  mode.init.transition();
}

// Main

void loop(void)
{
  cmd.readSerial();
  
  tsTimer.run();
  
  for (int i = 0; i < _fsm.count; ++i)
    _fsm[i].update();
}


