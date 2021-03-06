#include "Thermal.h"
#include "Utils.h"

Thermal::Thermal(char pin, ThermalCallback callback)
: _err(false)
, _callback(callback)
, _t0(-273.5f)
, _t1(-273.5f)
, _tm(_t0)
, _dt(0.0f)
, _sp(20.0f)
, _ts(tsLow)
, _bus(OneWire(pin))
, _sensor(DallasTemperature(&_bus)) 
{
}

bool Thermal::error()
{
  return _err;
}

bool Thermal::setup(float sp)
{
  _sp = sp;
  
  _sensor.begin();

  if (!_sensor.getAddress(_address, 0))
  {
    _t0 = _tm = -274.0f;
    _err = true;
    return false;
  }

  _sensor.setResolution(_address, TS_RESOLUTION);
  _sensor.setWaitForConversion(false);
  _sensor.requestTemperatures();

  _next = millis() + TS_INTERVAL;

  return true;
}

void Thermal::setSetPoint(float sp)
{
  if (sp < 20.0)
    _sp = 20.0;
  else if (sp > 40.0)
    _sp = 40.0;
  else
    _sp = sp;
    
  if (_t0 > _sp)
    _ts = tsHigh;
    
  if (_t0 < _sp)
    _ts = tsLow;
    
  
  _ts = tsLow;
}

void Thermal::update()
{
  unsigned long m = millis();
  
  if (m < _next)
    return;

  _next = m + TS_INTERVAL;
  
  if (_err)
  {
    _callback();
    return;
  }
  
  _tm = _sensor.getTempCByIndex(0);
  _sensor.requestTemperatures();

  if (fabs(_tm - _t0) > 1.0)
  {
    _t0 = _t1 = _tm;
    _dt = 0.0f;
  }
  else
  {
    _t1 = _t0;
    _t0 = 0.7 * _t0 + 0.3 * _tm;
    _dt = _t0 - _t1;
  }
  
  if (_ts == tsLow && _t0 > _sp)
    _ts = tsHigh;
    
  if (_ts == tsHigh && _t0 < _sp - 1.0)
    _ts = tsLow;
  
  _callback();
}

