#pragma once
#include <Arduino.h>

struct CRGB {
  byte r;
  byte g;
  byte b;
};

inline CRGB hsvToRgb(uint8_t h, uint8_t s, uint8_t v) {
  CRGB rgb;
  uint8_t region, remainder, p, q, t;

  if (s == 0) {
    // Achromatic (grey)
    rgb.r = rgb.g = rgb.b = v;
    return rgb;
  }

  region = h / 43;  // Divide by 43 to scale 0-255 range into 0-6 regions.
  remainder = (h - (region * 43)) * 6;

  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:
      rgb.r = v;
      rgb.g = t;
      rgb.b = p;
      break;
    case 1:
      rgb.r = q;
      rgb.g = v;
      rgb.b = p;
      break;
    case 2:
      rgb.r = p;
      rgb.g = v;
      rgb.b = t;
      break;
    case 3:
      rgb.r = p;
      rgb.g = q;
      rgb.b = v;
      break;
    case 4:
      rgb.r = t;
      rgb.g = p;
      rgb.b = v;
      break;
    case 5:
      rgb.r = v;
      rgb.g = p;
      rgb.b = q;
      break;
  }
  return rgb;
}
