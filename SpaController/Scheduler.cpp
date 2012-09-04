#include "Scheduler.h"
#include "Utils.h"

#define h(_x) (_x * SECS_PER_HOUR)

const char *weekDayStr[] =
{
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

Scheduler::Scheduler
  ( ScheduleItem *items
  , int count
  , void (*setModeCallback)(eMode mode)
  )
: _items(items)
, _count(count)
, _setModeCallback(setModeCallback)
, _currentItem(NULL)
{}

void Scheduler::print()
{
  update();
  
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    for (int j = 0; j < 7; ++j)
    {
      if ((1 << (j + 1)) & si.weekdays)
        Serial << weekDayStr[i] << " ";
    }
      
    Serial << digital(si.startTime)
    << " " << digital(si.endTime)
    << " " << digital(si.period)
    << " " << si.prefDuty
    << " " << si.maxDuty;

    if (&si == _currentItem)
      Serial << " **";
      
    Serial << endl;
  }
}

void Scheduler::update()
{
  ScheduleItem &si = itemForTime(now());
  
  if (&si != _currentItem)
  {
    _currentItem = &si;
  }
}

ScheduleItem& Scheduler::itemForTime(time_t time)
{
  uint8_t dm = 1 << weekday(time);
  int hr = hour(time);
  
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    if (!(dm & si.weekdays))
      continue;
      
    if (hr > si.startTime && hr < si.endTime)
      return _items[i];
  }
}



