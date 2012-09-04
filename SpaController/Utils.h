#ifndef Utils_h
#define Utils_h

#include <Arduino.h>
#include <avr/pgmspace.h>

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

#endif

