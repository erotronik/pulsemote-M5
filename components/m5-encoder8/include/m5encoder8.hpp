#pragma once

// M5Stack Unit 8Encoder: eight knobs with push buttons, a toggle switch and nine RGB LEDs, on I2C.
// A task of its own polls it, so nothing else waits on the bus; changes come out on encoder8_events

#include <stdint.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

constexpr int encoder8_knobs = 8;
constexpr int encoder8_switch = encoder8_knobs;  // the toggle switch's input number

struct encoder8_event_t {
  uint8_t input;  // knob 0-7, or encoder8_switch
  int16_t turn;   // a knob's clicks, + clockwise. 0 for a press or the switch
  bool pressed;   // when turn is 0: the button is down, or the switch is on
};

extern QueueHandle_t encoder8_events;  // nullptr until encoder8_begin() has found one

// Looks for the unit, and if it's there starts polling it. Its 5V has to be on already
bool encoder8_begin(gpio_num_t sda, gpio_num_t scl, uint8_t address = 0x41);

// LED 0-7 is above each knob, 8 by the switch. Applied by the polling task, so it doesn't block
void encoder8_set_led(int led, uint8_t r, uint8_t g, uint8_t b);
void encoder8_dim_leds(bool dimmed);  // turn them all down, or back up, keeping their colours
