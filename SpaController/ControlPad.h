#include <Arduino.h>
#include <SPI.h>
#include "Utils.h"
#include "Button.h"
#include "Multiplex7Seg.h"

struct Buttons
{
  Button left, right, incr, decr;
};

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
        byte pump: 1;
        byte aux: 1;
        byte safety: 1;
        byte heat: 1; // 2
        byte DS3: 1; // 5
        byte DS1: 1; // 6
        byte DS2: 1; // 7
      };
    };
    byte U2;
    union
    {
      byte RLY4;
      struct
      {
        byte unused: 4;
        byte path: 1;
        byte head: 1;
        byte steps: 1;
        byte footwell: 1;
      };
    };
  };


  ControlPad(byte clk_, byte latch_, byte data_, Array<Buttons, Button> &buttons_);
  
  void setup();
  void update();
  void updateButtons();
  
  void clearButtonEvents();
  
  void clearLEDs();
  void writeLED(byte n, byte b);
  
  Data d;
  
  Multiplex7Seg &seg;
  
  byte clk, latch, data;
  
  ByteArray leds;
  
  Array<Buttons, Button> &buttons;
  bool buttonChanged;
};
