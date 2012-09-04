#ifndef Thermal_h
#define Thermal_h

#include <OneWire.h>
#include <DallasTemperature.h>

#define TS_RESOLUTION 12
//#define TS_INTERVAL (750 / 1 << (12 - TS_RESOLUTION))
#define TS_INTERVAL 1000

typedef void (*ThermalCallback)();

enum eHeatTriggerState { hLow, hHigh };

class Thermal
{
public:
  
  Thermal(char pin, ThermalCallback callback);

  bool setup();

  float temperature() { return _t0; }

  float setPoint() { return _sp; }
  void setSetPoint(float sp) { _sp = sp; }
  
  float rate() { return _dt; }
  
  eHeatTriggerState triggerState() { return _ts; }

  void update();

  void service();

protected:

  ThermalCallback _callback;
  ThermalCallback _error;

  float _t0, _t1, _tm, _dt, _sp;
  unsigned long _next;
  
  eHeatTriggerState _ts;

  OneWire _bus;
  DallasTemperature _sensor;
  DeviceAddress _address;
};



#endif
