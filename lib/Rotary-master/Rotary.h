/*
 * Rotary encoder library for Arduino.
 */

#ifndef Rotary_h
#define Rotary_h

// Enable this to emit codes twice per step.
// #define HALF_STEP

// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Counter-clockwise step.
#define DIR_CCW 0x20

class Rotary
{
  public:
    Rotary(char, char);
    /* if using a rotary encoder connected directly to the MCU: call this function */
    unsigned char process(unsigned char pin1State, unsigned char pin2State);

  private:
    unsigned char state;
    unsigned char pin1;
    unsigned char pin2;
};

#endif
 
