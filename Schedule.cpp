#include "Schedule.h"

const char *weekDayStr[] =
{
  "S", "M", "T", "W", "T", "F", "S"
};

Schedule::Schedule
  ( ScheduleItem *items
  , int count
  , int setPoint
  , void (*setModeCallback)(eMode mode)
  )
: _items(items)
, _count(count)
, _setPoint(setPoint)
, _setModeCallback(setModeCallback)
, _currentHour(-1)
, _currentItem(NULL)
{}

void Schedule::setSetPoint(int setPoint)
{
  _setPoint = setPoint;
  update();
}


void Schedule::printItems()
{
  update();
  
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    for (int j = 0; j < 7; ++j)
    {
      if (1 << (j + 1) & si.weekdays)
        Serial << weekDayStr[j] << " ";
    }
    
    Serial << si.startTime << " ";
    Serial << si.endTime << " ";
    Serial << si.period << " ";
    Serial << si.prefDuty << " ";
    Serial << si.maxDuty << " ";

    if (&si == _currentItem)
      Serial << "* ";
        
    Serial << endl;
  }
}

void Schedule::update()
{
  if (_currentHour != hour())
  {
    _currentHour == hour();
    
    ScheduleItem &si = itemForTime(now());
    
    if (_currentItem != &si)
    {
      _currentItem = &si;

      _period = si.period * SECS_PER_HOUR;
      _prefDuty = si.prefDuty * _period / 100;
      _maxDuty = si.maxDuty * _period / 100;

      _dutyOff = 0;
      _dutyOn = 0;
    }
  }
  
  if (_maxDuty == 0)
  {
    // turn it off
  }
}

ScheduleItem& Schedule::itemForTime(time_t time)
{
  uint8_t dm = 1 << weekday(time);
  int hr = hour(time);
  
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    if (!(dm & si.weekdays))
      continue;
      
    if (hr > si.startTime && hr < si.endTime)
    {
      return _items[i];
    }
  }
}



