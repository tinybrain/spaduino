#include "Multiplex7Seg.h"

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

void Multiplex7Seg::mapSegments(byte &segments)
{
  byte mapped = 0;
  
  Serial.print("sm count ");
  Serial.println(segmentMap.count);
  
  for (byte n = 0; n < segmentMap.count; ++n)
  {
    if(segments & (1 << n))
      mapped |= (1 << segmentMap[n]);
  }
  
  segments = ~mapped;
}

void Multiplex7Seg::mapCharacter(byte &c)
{
  if (c >= '0' && c <= '9')
  {
    c = segmentLUT[c - '0'];
    return;
  }
  
  if (c >='a' && c <= 'z')
  {
    
    c = segmentLUT[c - 'a' + 10];
    return;
  }

  if (c >='A' && c <= 'Z')
  {
    c = segmentLUT[c - 'A' + 10];
  }
  
  switch(c)
  {
    case ' ': c = 0x00; break;
    case '-': c = 0x40; break;
    case '_': c = 0x08; break;
    case '=': c = 0x48; break;
    case '.': c = 0x80; break;
  }  
}

void dumpByte_(char *name, byte b)
{
  Serial.print(name);
  
  int i = 0;
  
  do { Serial.print((b & 1 << i) ? "1" : "0"); }
  while (i++ < 8);
  
  Serial.print("\n");
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
      // null terminated, pad with 0
      
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
        //digits[d - 1] = 0xFF;
        digits[d - 1] &= ~(1 << 2);
      }
    }
    else
    {
      mapCharacter(c);
      mapSegments(c);
      digits[d] = c;
      d++;
    }
  }
}

void Multiplex7Seg::setValue(int val)
{
  char *result = itoa(val, buffer, 10);
  
  encodeString(result);
}

void Multiplex7Seg::setTestSegment(byte segment)
{
  byte segments = 0 | 1 << segment;
  mapSegments(segments);
  
  for (int i = 0; i < digitMap.count; ++i)
  {
    digits[i] = segments;
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
