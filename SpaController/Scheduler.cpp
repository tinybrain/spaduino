#include "Scheduler.h"
#include "Utils.h"

#define h(_x) (_x * SECS_PER_HOUR)

Scheduler::Scheduler
  ( ScheduleItem *items
  , int count
  )
: _items(items)
, _count(count)
, _currentItem(NULL)
, _cycleStart(0)
, _dutyStart(0)
, _dutyAccum(0)
, _manualDuration(0)
{}

void Scheduler::reset()
{
  _cycleStart = now();
  _dutyStart = 0;
  _dutyAccum = 0;
  _manualDuration = 0;
}

void Scheduler::manual(long duration)
{
  reset();
  
  _manualDuration = duration;
  
  startDutyTimer();
}

time_t Scheduler::remaining()
{
  if (!_manualDuration)
    return 0;
    
  return _manualDuration - cycleElapsed();
}

time_t Scheduler::cycleElapsed()
{
  return now() - _cycleStart;
}

time_t Scheduler::dutyElapsed()
{
  time_t de = _dutyStart ? now() - _dutyStart : 0;
  de += _dutyAccum;
  return de;
}

ScheduleItem& Scheduler::itemForTime(time_t time)
{
  uint8_t dm = 1 << weekday(time);
  time_t dt = elapsedSecsToday(time);
 
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    if (!(dm & si.weekdays))
      continue;
      
    if (dt >= si.startTime && dt < si.endTime)
      return _items[i];
  }
  
  return _items[0];
}

void Scheduler::update()
{
  if (timeStatus() == timeNotSet || _manualDuration)
    return;
    
  // schedule

  ScheduleItem &si = itemForTime(now());
  
  if (&si != _currentItem)
  {
    _currentItem = &si;
    reset();
    return;
  }
  
  // cycle
  
  if (cycleElapsed() > _currentItem->period)
  {
    reset();
    return;
  }
}

void Scheduler::startDutyTimer()
{
  if (_dutyStart)
    return;
  
  _dutyStart = now();
}

void Scheduler::stopDutyTimer()
{
  if (!_dutyStart)
    return;
    
  _dutyAccum += now() - _dutyStart;  
  _dutyStart = 0;
}

eDutyState Scheduler::dutyState()
{
  if (_manualDuration)
    return (cycleElapsed() < _manualDuration) ? dsUnder : dsOver;
    
  time_t de = dutyElapsed();
  
  if (de < _currentItem->minDuty)
    return dsUnder;
    
  if (de >= _currentItem->maxDuty)
    return dsOver;
  
  return dsMet;
}




