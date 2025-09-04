#include "HallEffectSecurity.h"

HallEffectSecurity::HallEffectSecurity(uint8_t _dataPIN,uint8_t _powerPIN) : dataPIN(_dataPIN),powerPIN(_powerPIN), _lastFlux(0.0) {}

void HallEffectSecurity::begin() {
  pinMode(dataPIN, INPUT);
  pinMode(powerPIN,OUTPUT);
  digitalWrite(powerPIN,HIGH);
  // Perform initial reading to stabilize ADC
  for (int i = 0; i < 5; i++) {
    analogRead(dataPIN);
    delay(10);
  }
}

float HallEffectSecurity::readFlux() {
  int adcValue = analogRead(dataPIN);
  _lastFlux = adcToFlux(adcValue);
  return _lastFlux;
}

float HallEffectSecurity::adcToFlux(int adcValue) {
  // Convert ADC to voltage (0–3.3V for 0–4095)
  float voltage = (adcValue / 4095.0) * 3.3;
  // Convert voltage to flux (assuming midpoint 1.65V = 0 µT)
  float flux = (voltage - 1.65) / VOLT_PER_UT;
  return flux;
}

float HallEffectSecurity::calculateVariance(float readings[], int count) {
  if (count < 2) return 0.0;
  
  float sum = 0.0;
  for (int i = 0; i < count; i++) {
    sum += readings[i];
  }
  float mean = sum / count;
  
  float sumSquaredDiff = 0.0;
  for (int i = 0; i < count; i++) {
    float diff = readings[i] - mean;
    sumSquaredDiff += diff * diff;
  }
  return sumSquaredDiff / count;
}

HallEffectSecurity::SystemState HallEffectSecurity::getSystemState() {
  // Take multiple readings to calculate variance
  float readings[SAMPLE_COUNT];
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    readings[i] = readFlux();
    delay(10);  // Short delay between readings
  }
  
  // Calculate variance to detect unstable readings
  float variance = calculateVariance(readings, SAMPLE_COUNT);
  
  // Get average flux
  float sum = 0.0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += readings[i];
  }
  float avgFlux = sum / SAMPLE_COUNT;
  
  // Determine state
  if (variance > VARIANCE_THRESHOLD) {
    return SYSTEM_UNDER_ATTACK;  // High variance indicates attack
  }
  
  if (avgFlux >= OK_FLUX_MIN && avgFlux <= OK_FLUX_MAX) {
    return SLIDER_OK;
  }
  
  if (avgFlux >= MISSING_FLUX_MIN && avgFlux <= MISSING_FLUX_MAX) {
    return SLIDER_MISSING;
  }
  
  // If outside expected ranges and stable, assume attack
  return SYSTEM_UNDER_ATTACK;
}

float HallEffectSecurity::getFlux() {
  return _lastFlux;
}