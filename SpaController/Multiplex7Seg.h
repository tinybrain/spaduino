#ifndef MULTIPLEX7SEG_H
#define MULTIPLEX7SEG_H

#include "Arduino.h"
#include "Utils.h"

#ifndef MAX_DIGITS
#define MAX_DIGITS 3
#endif

class Multiplex7Seg 
{
public:

  Multiplex7Seg(ByteArray &digitMap_, ByteArray &segmentMap_, ByteArray &segmentLUT_);
  
  void mapSegments(byte &segments);
  void mapCharacter(byte &c);
  void encodeString(char *s);
  void setValue(int value);
  void setTestSegment(byte segment);
  
  void nextDigit(byte &U1, byte &U2);
  
  ByteArray &digitMap;
  ByteArray &segmentMap;
  ByteArray &segmentLUT;

  byte digits[MAX_DIGITS];
  char buffer[MAX_DIGITS * 2];
  byte current;
};

#endif
