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
  0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x3d, 0x76, 0x30, 0x1e, 0x00, 0x38, 0x00, 0x37, 0x3f, 0x73, 0x67, 0x50, 0x6d, 0x78, 0x3e, 0x00, 0x00, 0x00, 0x6e, 0x00
};

ByteArray segmentLUT(segmentLUT_, sizeof(segmentLUT));

Multiplex7Seg seg_(digitMap, segmentMap, segmentLUT);

ControlPad::ControlPad(byte clk_, byte latch_, byte data_)
: seg(seg_)
, clk(clk_)
, latch(latch_)
, data(data_)
, leds(ByteArray(leds_, sizeof leds_))
{
  // clear
  d.U1 = d.U2 = 0x00;
  
  // leds off
  d.LD1 = d.LD2 = d.LD3 = d.LD4 = 1;
}

void dumpByte(char *name, byte b)
{
  Serial.print(name);
  
  int i = 0;
  
  do { Serial.print((b & 1 << i) ? "1" : "0"); }
  while (i++ < 8);
  
  Serial.print("\n");
}

void ControlPad::dump()
{
  Serial.println("-----");
  // Serial.print("leds "); Serial.println(leds.count);
  // Serial.print("c "); Serial.print(clk);
  // Serial.print(" d "); Serial.print(data);
  // Serial.print(" l "); Serial.println(latch);
  dumpByte("U1 ", d.U1);
  dumpByte("U2 ", d.U2);
}

void ControlPad::setup()
{
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
  
  /*
  digitalWrite(latch, 0);
  shiftOut(data, clk, MSBFIRST, d.U2);
  shiftOut(data, clk, MSBFIRST, d.U1);
  digitalWrite(latch, 1);
  */
  digitalWrite(latch, 0);
  SPI.transfer(d.U2);
  SPI.transfer(d.U1);
  digitalWrite(latch, 1);
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
