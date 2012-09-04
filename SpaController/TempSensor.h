#ifndef TempSensor_h
#define TempSensor_h

#include <OneWire.h>
#include <DallasTemperature.h>

#define TS_RESOLUTION 12
#define TS_INTERVAL (750 / 1 << (12 - TS_RESOLUTION))

typedef void (*TempSensorCallback)();

class TempSensor
{
public:
  
  TempSensor(char pin, TempSensorCallback callback);

  bool setup();

  float temperature() { return _t; }

  void update();

  void service();

protected:

  TempSensorCallback _callback;
  TempSensorCallback _error;

  float _t;
  uint32_t _next;

  OneWire _bus;
  DallasTemperature _sensor;
  DeviceAddress _address;
};



#endif
