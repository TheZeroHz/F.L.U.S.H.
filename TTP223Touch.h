#ifndef TTP223TOUCH_H
#define TTP223TOUCH_H

#include <Arduino.h>

class TTP223Touch {
private:
  uint8_t _pin;                    // Pin connected to TTP223 OUT
  unsigned long _debounceDelay;    // Debounce delay in ms
  unsigned long _holdTime;         // Time to consider a hold in ms
  unsigned long _lastChange;       // Last state change time
  unsigned long _touchStart;       // Time when touch started
  bool _currentState;              // Current debounced state
  bool _lastRawState;              // Last raw pin state
  bool _isTouched;                 // Tracks touch state
  bool _isHeld;                    // Tracks hold state

public:
  // Constructor
  TTP223Touch(uint8_t pin, unsigned long debounceDelay = 50, unsigned long holdTime = 1000) {
    _pin = pin;
    _debounceDelay = debounceDelay;
    _holdTime = holdTime;
    _lastChange = 0;
    _touchStart = 0;
    _currentState = false;
    _lastRawState = false;
    _isTouched = false;
    _isHeld = false;
  }

  // Initialize the sensor
  void begin() {
    pinMode(_pin, INPUT);
  }
  void reset(){
    _lastChange = 0;
    _touchStart = 0;
    _currentState = false;
    _lastRawState = false;
    _isTouched = false;
    _isHeld = false;
    }
  // Update sensor state, call in loop()
  void update() {
    bool rawState = digitalRead(_pin) == HIGH;

    // Detect state change with debounce
    if (rawState != _lastRawState) {
      _lastChange = millis();
    }

    if (millis() - _lastChange >= _debounceDelay) {
      if (rawState != _currentState) {
        _currentState = rawState;

        if (_currentState) {
          // Touch started
          _touchStart = millis();
          _isTouched = true;
          _isHeld = false;
        } else {
          // Touch released
          _isTouched = false;
          _isHeld = false;
        }
      }
    }

    // Check for hold if touched
    if (_currentState && !_isHeld && (millis() - _touchStart >= _holdTime)) {
      _isHeld = true;
    }

    _lastRawState = rawState;
  }

  // Check if sensor is touched (momentary)
  bool isTouched() {
    return _isTouched && !_isHeld;
  }

  // Check if sensor is held
  bool isHeld() {
    return _isHeld;
  }
};

#endif
