#include "ControlPad.h"

byte leds_[] = { 2, 4, 1, 3 };

ByteArray leds_array(leds_, sizeof leds_);

byte digitMap_[] = { 5, 7, 6 };

ByteArray digitMap(digitMap_, sizeof(digitMap_));

byte segmentMap_[] = { 7, 4, 1, 3, 6, 5, 0, 2 };

ByteArray segmentMap(segmentMap_, sizeof(segmentMap_));

byte segmentLUT_[] = 
{
  // 0-9
  0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f,
  // A-Z
  0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x3d, 0x76, 0x30, 0x1e, 0x00, 0x38, 0x00, 0x37, 0x3f, 0x73, 0x67, 0x50, 0x6d, 0x78, 0x3e, 0x00, 0x00, 0x00, 0x6e, 0x00,
};

ByteArray segmentLUT(segmentLUT_, sizeof(segmentLUT));

Multiplex7Seg seg_(digitMap, segmentMap, segmentLUT);

ControlPad::ControlPad(byte clk_, byte latch_, byte data_, Array<Buttons, Button> &buttons_)
: seg(seg_)
, clk(clk_)
, latch(latch_)
, data(data_)
, leds(ByteArray(leds_, sizeof leds_))
, buttons(buttons_)
, buttonChanged(false)
{
  // clear
  d.U1 = d.U2 = 0x00;
  
  // leds off
  d.safety = d.pump = d.heat = d.aux = 1; // inverted
  d.footwell = d.steps = d.head = d.path = 0;
}

void ControlPad::setup()
{
  for (byte i = 0; i < buttons.count; ++i)
    buttons[i].setup(i);
  
  pinMode(clk, OUTPUT);
  pinMode(data, OUTPUT);
  pinMode(latch, OUTPUT);
  
  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  
  update();
}

void ControlPad::update()
{
  seg.nextDigit(d.U1, d.U2);
  
  digitalWrite(latch, 0);
  SPI.transfer(d.U2);
  SPI.transfer(d.U1);
  SPI.transfer(d.RLY4);
  digitalWrite(latch, 1);
}

void ControlPad::updateButtons()
{
  for (byte i = 0; i < buttons.count; ++i)
  {
    buttons[i].update();
    if (buttons[i].changed)
      buttonChanged = true;
  }
}

void ControlPad::clearButtonEvents()
{
  for (byte i = 0; i < buttons.count; ++i)
  {
    buttons[i].clear();
  }
  buttonChanged = false;
}

void ControlPad::clearLEDs()
{
  byte i = 4;
  while (--i) writeLED(leds[i], 0);
}

void ControlPad::writeLED(byte n, byte b)
{
  bitWrite(d.U1, leds[n], !b);
}
