#include <Time.h>
#include <SimpleTimer.h>
#include <Streaming.h>
#include <SerialCommand.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FiniteStateMachine.h>

// --------------------------------------------------------------------

// Main error

enum Status { Ok, Unknown, TemperatureSensor, WaterLevelSensor, HighLimit };
Status err = Ok;

// Serial Commands

#define ACK           "ack"
#define UNKNOWN       "unknown"
#define ERROR         "error"
#define NOT_READY     "not ready"
#define INVALID_ARG   "invalid arg"

SerialCommand cmd;

// Temperature

float temperature = 0.0;

OneWire tsBus(12);
DallasTemperature ts(&tsBus);
DeviceAddress tsAddress;

const int tsResolution = 12;
const int tsInterval = 750 / 1 << (12 - tsResolution);

SimpleTimer tsTimer;

// Water level sensor


// Heater element AC sense / High limit cutout


// Relays

struct Relays
{
  int safety, pump, heat, aux;
};

Relays relays = 
{
  8, 9, 10, 11
};

const int *rp = &relays.safety;
const int numRelays = sizeof(Relays) / sizeof(int);

// FSM list

struct FSMS
{
  FSM mode, pump, aux;
};

FSMS f =
{
   FSM("mode"), FSM("pump"), FSM("aux")
};

FSM *fsms = &f.mode;
const int numFSMs = sizeof(FSMS) / sizeof(FSM&);

// Mode FSM

struct Mode
{
  S error, init, off, filter, autoheat, rapidheat, soak; 
};

Mode mode = 
{
  S("error"     , enterModeError),
  S("init"      , enterModeInit, updateModeInit),
  S("off"       , enterModeOff),
  S("filter"    , enterModeFilter),
  S("autoheat"  , enterModeAutoHeat),
  S("rapidheat" , enterModeRapidHeat),
  S("soak"      , enterModeSoak)
};

// Pump FSM

struct Pump
{
  S error, off, on, heat;
};

Pump pump =
{
  S("error"     , enterPumpError),
  S("off"       , enterPumpOff),
  S("on"        , enterPumpOn),
  S("heat"      , enterPumpHeat)
};

const int numPumpStates = sizeof(pump) / sizeof(S);

// Aux FSM

struct Aux
{
  S off, on;
};

Aux aux =
{
  S("off"       , enterAuxOff),
  S("on"        , enterAuxOn)
};

const int numAuxStates = sizeof(aux) / sizeof(S);

// Setup

void setupFSMs()
{
  f.mode.setup(sizeof(mode) / sizeof(S), (S*)&mode, stateLog);
  f.pump.setup(sizeof(pump) / sizeof(S), (S*)&pump, stateLog);
  f.aux.setup(sizeof(aux) / sizeof(S), (S*)&aux, stateLog);
}

// --------------------------------------------------------------------

// Serial Commands

void setupSerialCommands()
{
  cmd.setDefaultHandler(onUnknown);

  cmd.addCommand("hello", sendAck);
  cmd.addCommand("error", sendError);
  cmd.addCommand("states", sendStates);
  cmd.addCommand("time", onTime);
  cmd.addCommand("temp", sendTemperature);
  cmd.addCommand("relays", sendRelays);
  cmd.addCommand("mode", onMode);
  cmd.addCommand("aux", onAux);
}

// log

void fsmLog(const char *action, FSM &f)
{
  Serial << action << " " << f.name() << endl;
}

void stateLog(const char *action, S &s)
{
  Serial << action << " " << s.name() << endl;
}

// Receive

void onUnknown(const char *cmd)
{
  Serial << UNKNOWN << endl;
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
  
  Serial << "timearg " << arg << endl;
  
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
  
  if (!f.mode.currentState()) { sendNotReady(); return; }
  
  char *arg = cmd.next();
  
  if (!arg)
  {
    sendMode();
    return;
  }
  
  Serial << "modearg " << arg << endl;
  
  State *s = f.mode.states();
  int mi = -1;
  
  for (int i = 0; i < f.mode.numStates(); ++i)
  {
    if (!strcmp(arg, s[i].name()))
    {
      mi = i;
      break;
    }
  }
  
  if (mi < 0)
  {
    sendInvalidArg();
    return;
  }
  
  s[mi]();
}

void onAux()
{
  if (checkError(mode.error)) return;
  
  if (!f.aux.currentState()) { sendNotReady(); return; }

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
  Serial << ACK << endl;
}

void sendInvalidArg()
{
  Serial << INVALID_ARG << endl;
}

void sendError()
{
  Serial << ERROR << " 0x" << _HEX(err) << endl;
}

void sendStates()
{
  Serial << "states";
  
  for (int i = 0; i < numFSMs; ++i)
  {
    FSM &f = fsms[i];
    State *s = f.currentState();
    Serial << " " << f.name() << ":" << (s ? s->name() : "-");
  }

  Serial << endl;
}

void sendNotReady()
{
  Serial << NOT_READY << endl;
}

void sendTime()
{
  Serial << "time " << now() << " status " << timeStatus() << endl;
}

void sendTemperature()
{
  Serial << "temp " << _FLOAT(temperature, 3) << endl;
}

void sendRelays()
{
  Serial << "relays ";
  
  for (int i = 0; i < numRelays; ++i)
    Serial << " " << digitalRead(rp[i]);
  
  Serial << endl;
}

void sendMode()
{
  Serial << "mode " << f.mode.currentState()->name() << endl;
}

// --------------------------------------------------------------------

// Clock

time_t requestSync()
{
  Serial << "sync" << endl;
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
  for (int i = 0; i < numRelays; ++i)
  {
    int r = rp[i];
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

void enterModeError(S &s)
{
  if (!pump.error.current())
    pump.error.immediate();
}

void enterModeInit(S &s)
{
  setupSerialCommands();
  
  setupIO();
  
  setupTemperatureSensor();
  
  if (err)
    mode.error.immediate();
}

void updateModeInit(S &s)
{
  if (timeStatus() == timeNotSet)
    return;
  
  mode.off();  
}

void enterModeOff(S &s)
{
  pump.off();
  aux.off();
}

void enterModeFilter(S &s)
{
  pump.on();
}

void enterModeAutoHeat(S &s)
{
  Serial << "unimplemented" << endl;
  mode.off();
}

void enterModeRapidHeat(S &s)
{
  aux.off();
  pump.heat();
}

void enterModeSoak(S &s)
{
  Serial << "unimplemented" << endl;
  mode.off();
}

// --------------------------------------------------------------------

// Pump FSM Handlers

void enterPumpError(S &s)
{
  aux.off.immediate();
  
  setPumpRelays(0, 0, 0);
}

void enterPumpOff(S &s)
{
  setPumpRelays(1, 0, 0);
}

void enterPumpOn(S &s)
{
  setPumpRelays(1, 1, 0);
}

void enterPumpHeat(S &s)
{
  setPumpRelays(1, 1, 1);
}

void updatePumpHeat(S &s)
{
  if (!err)
    return;
    
  pump.error.immediate();
}

// --------------------------------------------------------------------

// Aux FSM Handlers

void toggleAux()
{
  if (aux.on.current())
    aux.off();
  else
    aux.on();
}

void enterAuxOff(S &s)
{
  setAuxRelay(0);
  sendRelays();
}

void enterAuxOn(S &s)
{
  if (pump.heat.current())
    pump.on.immediate();
    
  setAuxRelay(1);
  sendRelays();
}

// --------------------------------------------------------------------

// Setup

void setup(void)
{
  Serial.begin(9600);

  Serial << "ok, schedule me some bubbles!" << endl;

  setupFSMs();

  setSyncProvider(requestSync);
  
  mode.init();
}

// Main

void loop(void)
{
  cmd.readSerial();
  
  tsTimer.run();

  f.mode.update();
  f.pump.update();
  f.aux.update();  
}


