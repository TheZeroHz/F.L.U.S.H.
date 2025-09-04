#ifndef CABLE_H
#define CABLE_H

#include <Arduino.h>

class Cable {
  private:
    uint8_t adc_pin;
    bool stable = true;
    int frequency;
    unsigned long max_read_duration = 5000;
    int threshold = 3000;
    int samples = 10;
    const char* thresh_path = "/cable_thresh.txt";

    int getAverageReading();
    void saveThresholdToSPIFFS();
    bool loadThresholdFromSPIFFS();

  public:
    Cable(uint8_t adc, int freq, int avg_samples = 10);
    bool IsConnected();
    bool IsStable();
    void calibrateOnConnect(float margin = 0.9);
};

#endif
