#include "Cable.h"
#include <SPIFFS.h>

Cable::Cable(uint8_t adc, int freq, int avg_samples) {
  adc_pin = adc;
  frequency = freq;
  samples = avg_samples;

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Try to load from SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  } else {
    if (!loadThresholdFromSPIFFS()) {
      Serial.println("No saved threshold found. Need calibration.");
    } else {
      Serial.printf("Loaded threshold: %d\n", threshold);
    }
  }
}

int Cable::getAverageReading() {
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += analogRead(adc_pin);
    vTaskDelay(2);
  }
  return sum / samples;
}

bool Cable::IsConnected() {
  return getAverageReading() > threshold;
}

bool Cable::IsStable() {
  unsigned long start_time = millis();
  stable = true;

  while (millis() - start_time < max_read_duration) {
    if (getAverageReading() < threshold) {
      stable = false;
      break;
    }
    vTaskDelay(1000 / frequency);
  }

  return stable;
}

void Cable::calibrateOnConnect(float margin) {
  int avg = getAverageReading();
  threshold = avg * margin;
  Serial.printf("Calibrated threshold: %d\n", threshold);
  saveThresholdToSPIFFS();
}

void Cable::saveThresholdToSPIFFS() {
  File file = SPIFFS.open(thresh_path, FILE_WRITE);
  if (file) {
    file.printf("%d\n", threshold);
    file.close();
    Serial.println("Threshold saved to SPIFFS.");
  } else {
    Serial.println("Failed to save threshold.");
  }
}

bool Cable::loadThresholdFromSPIFFS() {
  File file = SPIFFS.open(thresh_path, FILE_READ);
  if (file) {
    String line = file.readStringUntil('\n');
    file.close();
    threshold = line.toInt();
    return threshold > 0;
  }
  return false;
}
