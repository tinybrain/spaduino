#include "Multiplex7Seg.h"

char dial[] =
{
  //0x20, 0x30, 0x10, 0x18, 0x08, 0x0c, 0x04, 0x06, 0x02, 0x03, 0x40, 0x21
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20
};

// digitPins - LSB to MSB, segmentPins - a to g
Multiplex7Seg::Multiplex7Seg(ByteArray &digitMap_, ByteArray &segmentMap_, ByteArray &segmentLUT_)
: digitMap(digitMap_)
, segmentMap(segmentMap_)
, segmentLUT(segmentLUT_)
, current(0)
{
  //for (byte i = 0; i < segmentLUT.count; ++i)
  //{
  //  mapSegments(segmentLUT[i]);
  //}  
}

byte Multiplex7Seg::mapSegments(byte segments)
{
  byte mapped = 0;
  
  for (byte n = 0; n < segmentMap.count; ++n)
  {
    if(segments & (1 << n))
      mapped |= (1 << segmentMap[n]);
  }
  
  return ~mapped;
}

byte Multiplex7Seg::mapCharacter(byte c)
{
  if (c >= '0' && c <= '9')
    return segmentLUT[c - '0'];
  
  if (c >='a' && c <= 'z')
    return segmentLUT[c - 'a' + 10];

  if (c >='A' && c <= 'Z')
    return segmentLUT[c - 'A' + 10];
  
  switch(c)
  {
    case ' ': return 0x00;
    case '^': return 0x02;
    case ',': return 0x04;
    case '-': return 0x40;
    case '_': return 0x08;
    case '=': return 0x48;
    case '.': return 0x80;
  }
  
  return 0x00;
}

void Multiplex7Seg::encodeString(char *s)
{
  byte d = 0;
  byte n = 0;
  
  for (; d < digitMap.count; ++n)
  {
    byte c = s[n];
    
    if (c == 0)
    {
      for(; d < digitMap.count; ++d)
      {
        digits[d] = 0;
      }
      break;
    }
    
    if(c == '.')
    {
      if (d > 0)
      {
        digits[d - 1] &= ~(1 << 2);
      }
    }
    else
    {
      digits[d] = mapSegments(mapCharacter(c));
      d++;
    }
  }
}

void Multiplex7Seg::setValue(int val)
{
  char *result = itoa(val, buffer, 10);
  
  encodeString(result);
}

void Multiplex7Seg::setFloat(float value)
{
  char buffer[6];
  char *sp = buffer;
  
  int vi = (int)(value *= 10);
  
  int i1 = vi / 100;
  vi %= 100;
  
  int i2 = vi / 10;
  vi %= 10;
  
  if (i1)
    *sp++ = '0' + i1;
  else
    *sp++ = ' ';
    
  *sp++ = '0' + i2;
  *sp++ = '.';
  
  *sp++ = '0' + vi;
  
  *sp++ = 0;
  
  encodeString(buffer);  
}

void Multiplex7Seg::setSwitch(int value)
{
  digits[2] = mapSegments(value ? 0x01 : 0x08);
}

void Multiplex7Seg::setTimer(time_t secs, bool dot)
{
  int mins = secs / 60;
  secs %= 60;
  
  if (mins > 60)
  {
    encodeString("OFL");
  }
  else
  {
    char *sp = buffer;
    *sp++ = ' ';
    
    if (dot)
      *sp++ = '.';
    
    *sp++ = (mins / 10) + '0';
    *sp++ = (mins % 10) + '0';

      
    *sp++ = 0;
    
    encodeString(buffer);
    
    /*
    
    byte secdial = dial[secs / 10];
    
    digits[2] = mapSegments(secdial);
    */
  }
}

void Multiplex7Seg::nextDigit(byte&U1, byte &U2)
{
  // digit select
  
  bitClear(U1, digitMap[current]);
  
  if (++current >= digitMap.count)
    current = 0;
    
  bitSet(U1, digitMap[current]);
  
  // segments
  
  U2 = digits[current];
}
