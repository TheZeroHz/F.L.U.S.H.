#ifndef HALL_EFFECT_SECURITY_H
#define HALL_EFFECT_SECURITY_H

#include <Arduino.h>

class HallEffectSecurity {
public:
  // Enum for system states
  enum SystemState {
    SLIDER_OK,
    SLIDER_MISSING,
    SYSTEM_UNDER_ATTACK
  };

  // Constructor: takes the analog pin number
  HallEffectSecurity(uint8_t _dataPIN,uint8_t _powerPIN);

  // Initialize the sensor
  void begin();
  // Get the current system state
  SystemState getSystemState();

  // Get the latest flux reading in microtesla (µT)
  float getFlux();

private:
  uint8_t dataPIN,powerPIN;
  float _lastFlux;
  
  // Calibration parameters (based on provided data)
  static constexpr float OK_FLUX_MIN = 65.50;  // µT
  static constexpr float OK_FLUX_MAX = 69.50;  // µT
  static constexpr float MISSING_FLUX_MIN = -15;  // µT
  static constexpr float MISSING_FLUX_MAX = 15;  // µT
  static constexpr float VARIANCE_THRESHOLD = 4.5;  // µT^2 for attack detection
  static constexpr int SAMPLE_COUNT = 100;  // Number of samples for variance
  static constexpr int ADC_MIDPOINT = 2048;  // 12-bit ADC midpoint (1.65V)
  static constexpr float VOLT_PER_UT = 0.01;  // Approx 10mV/µT (tuned to data)
  
  // Internal methods
  float readFlux();
  float calculateVariance(float readings[], int count);
  float adcToFlux(int adcValue);
};

#endif