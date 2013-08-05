#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

enum ButtonState
{
  ButtonUp = LOW, ButtonDown = HIGH
};

class Button
{
public:

  Button(byte pin_);
  
  void setup(byte index_);
  
  void update();
  void clear();
  
  bool down() { return (changed && (state == ButtonDown)); }
  bool up() { return (changed && (state == ButtonUp)); }

  byte pin;
  byte index;
  
  byte state;
  byte lastState;

  static int debounceDelay;
  int debounce;
  boolean changed;
};

#endif
