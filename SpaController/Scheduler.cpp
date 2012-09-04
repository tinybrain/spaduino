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
  )
: _items(items)
, _count(count)
, _currentItem(NULL)
, _cycleStart(0)
, _dutyOnTimer(0)
, _dutyOnLastUpdate(0)
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
        Serial << weekDayStr[j] << " ";
    }
      
    Serial << digital(si.startTime)
    << " " << digital(si.endTime)
    << " " << digital(si.period)
    << " " << si.minDuty
    << " " << si.maxDuty;

    if (&si == _currentItem)
      Serial << " **";
      
    Serial << endl;
  }
  
  Serial << "dtmr " << _dutyOnTimer << endl;
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

void Scheduler::update()
{
  if (timeStatus() == timeNotSet)
    return;
    
  time_t n = now();
    
  // schedule

  ScheduleItem &si = itemForTime(n);
  
  if (&si != _currentItem)
  {
    _currentItem = &si;
    resetCycle();
    return;
  }
  
  // cycle
  
  if (n > _cycleStart + _currentItem->period)
  {
    resetCycle();
    return;
  }
  
  // update duty
  
  updateDutyTimer(); 
}

void Scheduler::resetCycle()
{
  Serial << "rst" << endl;
  
  _cycleStart = now();
  _dutyOnTimer = 0;
  _dutyOnLastUpdate = 0;
}

void Scheduler::startDutyTimer()
{
  Serial << "gdt " << _dutyOnLastUpdate << endl;

  if (_dutyOnLastUpdate)
    return;
  
  _dutyOnLastUpdate = now();
}

void Scheduler::stopDutyTimer()
{
  Serial << "sdt " << _dutyOnLastUpdate << endl;

  if (!_dutyOnLastUpdate)
    return;
    
  updateDutyTimer();
    
  _dutyOnLastUpdate = 0;
}

void Scheduler::updateDutyTimer()
{
  if (!_dutyOnLastUpdate)
    return;
    
  time_t n = now();
  
  _dutyOnTimer += n - _dutyOnLastUpdate;
  _dutyOnLastUpdate = n;
}

eDutyState Scheduler::dutyState()
{
  if (_dutyOnTimer < _currentItem->minDuty)
    return dsUnder;
    
  if (_dutyOnTimer >= _currentItem->maxDuty)
    return dsOver;
  
  return dsMet;
}


