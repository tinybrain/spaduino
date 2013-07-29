#ifndef Utils_h
#define Utils_h

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

inline int freeRam ()
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

// --------------------------------------------------------------------

template <class S, class E>
class Array
{
public:

  Array(S &structure_)
  : structure(structure_)
  , data(reinterpret_cast<E*>(&structure))
  , count(sizeof(S) / sizeof(E))
  {}

  E& operator [](size_t i) { return data[i]; }

  S& structure;
  E* data;
  size_t count;
};

class ByteArray
{
public:
  
  ByteArray(byte data_[], size_t count_)
  : data(data_)
  , count(count_)
  {}
  
  byte &operator[](size_t i) { return data[i]; }
  
  byte *data;
  size_t count;
};

// --------------------------------------------------------------------

template <class T>
int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    byte i;
    for (i = 0; i < sizeof(value); i++)
	  EEPROM.write(ee++, *p++);
    return i;
}

template <class T>
int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    byte i;
    for (i = 0; i < sizeof(value); i++)
	  *p++ = EEPROM.read(ee++);
    return i;
}


#endif

