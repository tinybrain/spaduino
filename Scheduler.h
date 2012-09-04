#ifndef Schedule_h
#define Schedule_h

#include <Arduino.h>
#include <Time.h>
#include <Streaming.h>

enum eMode { Error, Init, Off, Autoheat, Rapidheat, Soak };

enum eHeatMode { hNone, hMinimum, hMaximum };
enum eWeekDay { Sun = 0x02, Mon = 0x04, Tue = 0x08, Wed = 0x10, Thu = 0x20 , Fri = 0x40 , Sat = 0x80 };

#define Weekdays  Mon | Tue | Wed | Thu | Fri
#define Weekend   Sun | Sat
#define AllWeek   Weekdays | Weekend

#define hr(_x) (_x * SECS_PER_HOUR)

struct ScheduleItem
{
  uint8_t  weekdays;
  uint16_t startTime;
  uint16_t endTime;
  uint16_t period;
  uint8_t prefDuty;
  uint8_t maxDuty;
};

class Scheduler
{
  public:  

  Scheduler
    ( ScheduleItem *items
    , int count
    , void(*setModeCallback)(eMode mode)
    );
  
  void print();
  
  void update();
  ScheduleItem& itemForTime(time_t time);
  
  ScheduleItem *_items;
  int _count;
  
  eMode _currentMode;
  void (*_setModeCallback)(eMode mode);
    
  ScheduleItem *_currentItem;
  
  time_t _period;
  time_t _prefDuty;
  time_t _maxDuty;
  time_t _dutyOn;
  time_t _dutyOff;
  
};

#endif
