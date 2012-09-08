#ifndef Schedule_h
#define Schedule_h

#include <Arduino.h>
#include <Time.h>
#include <Streaming.h>

enum eDutyState { dsUnder, dsMet, dsOver };

enum eWeekDay { Sun = 0x02, Mon = 0x04, Tue = 0x08, Wed = 0x10, Thu = 0x20 , Fri = 0x40 , Sat = 0x80 };

#define Weekdays  Mon | Tue | Wed | Thu | Fri
#define Weekend   Sun | Sat
#define AllWeek   Weekdays | Weekend

#define hr(_x) (_x * SECS_PER_HOUR)
#define mn(_x) (_x * SECS_PER_MIN)

struct ScheduleItem
{
  uint8_t  weekdays;
  uint16_t startTime;
  uint16_t endTime;
  uint16_t period;
  uint8_t minDuty;
  uint8_t maxDuty;
};

class Scheduler
{
  public:  

  Scheduler
    ( ScheduleItem *items
    , int count
    );
  
  void printSchedule();
  void printTimers();

  void reset();
  void manual(long duration);

  ScheduleItem& itemForTime(time_t time);
    
  void update();  

  time_t cycleElapsed();
  time_t dutyElapsed();
  
  void startDutyTimer();
  void stopDutyTimer();

  eDutyState dutyState();
  
protected:

  ScheduleItem *_items;
  int _count;
  
  ScheduleItem *_currentItem;
  
  time_t _cycleStart;
  time_t _dutyStart;
  time_t _dutyAccum;
  time_t _manualDuration;
};

#endif
