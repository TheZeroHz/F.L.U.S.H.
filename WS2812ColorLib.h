#ifndef WS2812ColorLib_h
#define WS2812ColorLib_h
#include <Arduino.h>
#include <Bonezegei_WS2812.h>

class WS2812ColorLib {
public:
  WS2812ColorLib(uint8_t pin);
  void begin();
  void setColor(uint32_t color);
  void setColorByName(const char* name);
  void setBrightness(float level);
  uint32_t getColorByName(const char* name);
  void blink(uint32_t color, unsigned long onTime, unsigned long offTime, int count);
  void blinkByName(const char* name, unsigned long onTime, unsigned long offTime, int count);
  void updateBlink();

private:
  Bonezegei_WS2812 rgb;
  float brightness;
  
  struct Color {
    uint32_t hex;
    const char* name;
  };
  
  static const Color colorLibrary[10];
  void applyBrightness(uint32_t color);
  
  // Blink state variables
  bool isBlinking;
  uint32_t blinkColor;
  unsigned long blinkOnTime;
  unsigned long blinkOffTime;
  int blinkCount;
  int currentBlink;
  bool blinkState;
  unsigned long lastBlinkTime;
};

#endif
