#include "Scheduler.h"
#include "Utils.h"

#define h(_x) (_x * SECS_PER_HOUR)

char fmtbuff[12];

char* dec2(char *p, byte value)
{
  *p++ = '0' + value / 10;
  *p++ = '0' + value % 10;
  return p;
}

char* digital(time_t t, boolean showSecs)
{
  char *p = fmtbuff;
  
  p = dec2(p, hour(t));
  *p++ = ':';
  p = dec2(p, minute(t));

  if (showSecs)
  {
    *p++ = ':';
    p = dec2(p, second(t));
  }
  
  *p++ = 0;

  return fmtbuff;
}

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

void Scheduler::printSchedule()
{
  update();
  
  Serial << "clk"
  << " " << dayStr(weekday())
  << " " << day()
  << " " << monthStr(month())
  << " " << year()
  << " " << digital(now(), false)
  << endl;
  
  for (int i = 0; i < _count; ++i)
  {
    ScheduleItem &si = _items[i];
    
    Serial << "sci"
    << " " << digital(si.startTime, false)
    << " " << digital(si.endTime, false)
    << " " << digital(si.period, false)
    << " " << digital(si.minDuty, true)
    << " " << digital(si.maxDuty, true);
    
    for (int j = 1; j < 8; ++j)
    {
      if ((1 << j) & si.weekdays)
        Serial << " " << dayShortStr(j);
    }
      
    if (&si == _currentItem)
      Serial << " **";
      
    Serial << endl;
  }
}

void Scheduler::printTimers()
{
  update();
  
  if (_currentItem)
  {
    Serial << "stm"
           << " " << _currentItem->period
           << " " << _currentItem->minDuty
           << " " << _currentItem->maxDuty
           << " " << _cycleStart
           << " " << _manualDuration
           << " " << cycleElapsed()
           << " " << dutyElapsed()
           << " " << dutyState()
           << endl;
  }

  //Serial << "cs " << _cycleStart << " ds " << _dutyStart << " da " << _dutyAccum << endl;
}

void Scheduler::reset()
{
  Serial << "rst" << endl;
  
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




