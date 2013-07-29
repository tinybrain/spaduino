#include <Arduino.h>
#include <SPI.h>
#include "Utils.h"
#include "Multiplex7Seg.h"

class ControlPad
{
public:
  
  struct Data
  {
    union
    {
      byte U1;
      struct
      {
        byte reserved: 1; // 0
        byte LD3: 1; // 1
        byte LD1: 1; // 2
        byte LD4: 1; // 3
        byte LD2: 1; // 4
        byte DS3: 1; // 5
        byte DS1: 1; // 6
        byte DS2: 1; // 7
      };
    };
    byte U2;
  };


  ControlPad(byte clk_, byte latch_, byte data_);
  
  void dump();
  void setup();
  void update();

  void clearLEDs();
  void writeLED(byte n, byte b);
  
  Data d;
  
  Multiplex7Seg &seg;
  
  byte clk, latch, data;
  
  ByteArray leds;
  
};
