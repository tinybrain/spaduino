#ifndef Schedule_h
#define Schedule_h

#include <Arduino.h>
#include <Time.h>
#include <Streaming.h>

enum eMode { mError, mInit, mOff, mAutoheat, mRapidheat, mSoak };
enum ePump { pError, pOff, pOn, pHeat };

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
  
  void print();

  ScheduleItem& itemForTime(time_t time);
  
  void update();  
  void resetCycle();
  
  void startDutyTimer();
  void stopDutyTimer();
  void updateDutyTimer();
  
  eDutyState dutyState();
  
protected:
  
  ScheduleItem *_items;
  int _count;
  
  ScheduleItem *_currentItem;
  
  time_t _cycleStart;
  time_t _dutyOnMin;
  time_t _dutyOnMax;
  time_t _dutyOnLastUpdate;
  time_t _dutyOnTimer;
};

#endif
