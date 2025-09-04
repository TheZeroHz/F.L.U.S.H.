#include "WS2812ColorLib.h"

const WS2812ColorLib::Color WS2812ColorLib::colorLibrary[10] = {
  {0xFF0000, "Red"},
  {0x00FF00, "Green"},
  {0x0000FF, "Blue"},
  {0xFFFF00, "Yellow"},
  {0xFF00FF, "Magenta"},
  {0x00FFFF, "Cyan"},
  {0xFFFFFF, "White"},
  {0xFFA500, "Orange"},
  {0x800080, "Purple"},
  {0xFFC0CB, "Pink"}
};

WS2812ColorLib::WS2812ColorLib(uint8_t pin) : rgb(pin) {
  brightness = 1.0;
  isBlinking = false;
  blinkColor = 0;
  blinkOnTime = 0;
  blinkOffTime = 0;
  blinkCount = 0;
  currentBlink = 0;
  blinkState = false;
  lastBlinkTime = 0;
}

void WS2812ColorLib::begin() {
  rgb.begin();
}

void WS2812ColorLib::applyBrightness(uint32_t color) {
  uint8_t r = ((color >> 16) & 0xFF) * brightness;
  uint8_t g = ((color >> 8) & 0xFF) * brightness;
  uint8_t b = (color & 0xFF) * brightness;
  rgb.setPixel((r << 16) | (g << 8) | b);
}

void WS2812ColorLib::setColor(uint32_t color) {
  isBlinking = false; // Stop blinking when setting a new color
  applyBrightness(color);
}

void WS2812ColorLib::setColorByName(const char* name) {
  uint32_t color = getColorByName(name);
  setColor(color);
}

void WS2812ColorLib::setBrightness(float level) {
  if (level >= 0.0 && level <= 1.0) {
    brightness = level;
  }
}

uint32_t WS2812ColorLib::getColorByName(const char* name) {
  for (int i = 0; i < 10; i++) {
    if (strcmp(name, colorLibrary[i].name) == 0) {
      return colorLibrary[i].hex;
    }
  }
  return 0x000000; // Return black if color not found
}

void WS2812ColorLib::blink(uint32_t color, unsigned long onTime, unsigned long offTime, int count) {
  isBlinking = true;
  blinkColor = color;
  blinkOnTime = onTime;
  blinkOffTime = offTime;
  blinkCount = count * 2; // Each blink has on and off state
  currentBlink = 0;
  blinkState = true;
  lastBlinkTime = millis();
  applyBrightness(color);
}

void WS2812ColorLib::blinkByName(const char* name, unsigned long onTime, unsigned long offTime, int count) {
  uint32_t color = getColorByName(name);
  blink(color, onTime, offTime, count);
}

void WS2812ColorLib::updateBlink() {
  if (!isBlinking) return;

  unsigned long currentTime = millis();
  unsigned long interval = blinkState ? blinkOnTime : blinkOffTime;

  if (currentTime - lastBlinkTime >= interval) {
    currentBlink++;
    blinkState = !blinkState;
    lastBlinkTime = currentTime;

    if (currentBlink >= blinkCount) {
      isBlinking = false;
      rgb.setPixel(0); // Turn off LED after blinking
      return;
    }

    if (blinkState) {
      applyBrightness(blinkColor);
    } else {
      rgb.setPixel(0); // Turn off during off state
    }
  }
}
