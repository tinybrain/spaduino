#include "TempSensor.h"

TempSensor::TempSensor(char pin, TempSensorCallback callback)
: _callback(callback)
, _t(-273.5)
, _bus(OneWire(pin))
, _sensor(DallasTemperature(&_bus)) 
{
}

bool TempSensor::setup()
{
  _sensor.begin();

  if (!_sensor.getAddress(_address, 0))
  {
    _t = -274.0;
    return false;
  }

  _sensor.setResolution(_address, TS_RESOLUTION);
  _sensor.setWaitForConversion(false);
  _sensor.requestTemperatures();

  _next = millis() + TS_INTERVAL;

  return true;
}

void TempSensor::update()
{
  if (millis() > _next)
    return;
    
  float nt = _sensor.getTempCByIndex(0);
  _sensor.requestTemperatures();
  _next = millis() + TS_INTERVAL;
  
  if (nt == _t)
    return;
    
  _t = nt;
  _callback();
}
