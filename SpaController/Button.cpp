#include "Button.h"

int Button::debounceDelay = 50;

Button::Button(byte pin_)
: 
pin(pin_)
, index(0)
, state(ButtonUp)
, lastState(ButtonUp)
, debounce(0)
{
}

void Button::setup(byte index_)
{
  index = index_;
  pinMode(pin, INPUT_PULLUP);
}

void Button::update()
{
  ButtonState newState = (digitalRead(pin) == HIGH) ? ButtonUp : ButtonDown;
  
  if (newState != lastState)
  {
    debounce = millis();
  }
  
  if ((millis() - debounce) > debounceDelay)
  {
    if (newState != state)
    {
      state = newState;
      changed = true;
    }
  }
  
  lastState = newState;
}

void Button::clear()
{
  changed = false;
}

