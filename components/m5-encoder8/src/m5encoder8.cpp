#include "m5encoder8.hpp"

#include <string.h>
#include <initializer_list>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/task.h>

// The unit's I2C is bit-banged, because its firmware is fussy, and hangs holding the bus low until
// it's powered off if it's not done its way:
// - No repeated start between writing the register number and reading it, which is how M5Unified and
//   ESP-IDF's driver read a register: it has to be a stop and a new start. ESP-IDF's driver fails
//   that way round too on the ESP32-P4, though its writes work
// - One register at a time: it hangs on reading past the end of one, such as all the counters at
//   once, or four of the buttons
// So a poll is 17 small reads, and the unit holds the clock low for a while on most of them as it
// fetches the value: the task sleeps then, rather than spinning

static const char *TAG = "encoder8";

namespace {

constexpr uint8_t REG_COUNTER = 0x00;  // 8 x int32, little endian
constexpr uint8_t REG_BUTTON = 0x50;   // 8 x byte, 0 while pressed
constexpr uint8_t REG_SWITCH = 0x60;   // byte, 1 when on
constexpr uint8_t REG_RGB = 0x70;      // 9 x r,g,b
constexpr uint8_t REG_FIRMWARE = 0xFE;

constexpr int poll_ms = 30;
constexpr int half_bit_us = 1;           // with the GPIO calls, about 300kHz
constexpr int stretch_spin_us = 20;      // how long to spin for the clock before sleeping instead
constexpr int stretch_timeout_ms = 50;   // and how long it may hold it in all: longer while it's busy
constexpr int counts_per_click = 2;      // what a knob's counter goes up by for each click
constexpr int lost_polls = 30;           // not answering this many times running, it's been unplugged

gpio_num_t sda, scl;
uint8_t addr;

void line(gpio_num_t g, bool high) { gpio_set_level(g, high); }  // open drain: high releases it
void wait() { esp_rom_delay_us(half_bit_us); }

// Release the clock, and wait while the unit holds it low
bool clock_high() {
  line(scl, 1);
  for (int t = 0; t < stretch_spin_us; t++) {
    if (gpio_get_level(scl)) return true;
    esp_rom_delay_us(1);
  }
  for (int t = 0; t < stretch_timeout_ms; t++) {
    vTaskDelay(1);
    if (gpio_get_level(scl)) return true;
  }
  return false;
}

void start() {
  line(sda, 1); clock_high(); wait();
  line(sda, 0); wait();
  line(scl, 0); wait();
}

void stop() {
  line(sda, 0); wait();
  clock_high(); wait();
  line(sda, 1); wait();
}

// Returns whether it was acknowledged
bool write_byte(uint8_t v) {
  for (int bit = 7; bit >= 0; bit--) {
    line(sda, v & (1 << bit)); wait();
    if (!clock_high()) return false;
    wait(); line(scl, 0);
  }
  line(sda, 1); wait();
  if (!clock_high()) return false;
  bool ack = !gpio_get_level(sda);
  wait(); line(scl, 0);
  return ack;
}

uint8_t read_byte(bool ack) {
  uint8_t v = 0;
  line(sda, 1);
  for (int bit = 0; bit < 8; bit++) {
    wait(); clock_high(); wait();
    v = (v << 1) | gpio_get_level(sda);
    line(scl, 0);
  }
  line(sda, !ack); wait();
  clock_high(); wait();
  line(scl, 0); line(sda, 1);
  return v;
}

bool write_reg(uint8_t reg, const uint8_t *data, int len) {
  start();
  bool ok = write_byte(addr << 1) && write_byte(reg);
  for (int i = 0; ok && i < len; i++) ok = write_byte(data[i]);
  stop();
  return ok;
}

bool read_reg(uint8_t reg, uint8_t *value) {
  start();
  bool ok = write_byte(addr << 1) && write_byte(reg);
  stop();  // not a repeated start: see the top
  if (!ok) return false;
  esp_rom_delay_us(10);
  start();
  ok = write_byte((addr << 1) | 1);
  if (ok) *value = read_byte(false);
  stop();
  return ok;
}

// A slave left part way through a byte can hold the data line low: clock it out of that
void bus_recover() {
  for (int i = 0; i < 9 && !gpio_get_level(sda); i++) {
    line(scl, 0); wait();
    clock_high(); wait();
  }
  stop();
}

uint8_t leds[9][3];
uint32_t leds_dirty = 0;
bool leds_dimmed = false;
portMUX_TYPE leds_mux = portMUX_INITIALIZER_UNLOCKED;

void write_leds() {
  uint32_t dirty;
  uint8_t rgb[9][3];
  bool dimmed;
  taskENTER_CRITICAL(&leds_mux);
  dirty = leds_dirty;
  leds_dirty = 0;
  memcpy(rgb, leds, sizeof(rgb));
  dimmed = leds_dimmed;
  taskEXIT_CRITICAL(&leds_mux);
  for (int i = 0; i < 9; i++) {
    if (!(dirty & (1u << i))) continue;
    if (dimmed)
      for (auto &v : rgb[i]) v /= 8;
    write_reg(REG_RGB + 3 * i, rgb[i], 3);
  }
}

struct reading {
  uint8_t counter[encoder8_knobs];  // the counters' low bytes: plenty to see how far they've turned
  uint8_t button[encoder8_knobs];
  uint8_t sw;
};

bool read_all(reading &r) {
  for (int i = 0; i < encoder8_knobs; i++)
    if (!read_reg(REG_COUNTER + 4 * i, &r.counter[i]) || !read_reg(REG_BUTTON + i, &r.button[i])) return false;
  return read_reg(REG_SWITCH, &r.sw);
}

void send(uint8_t input, int16_t turn, bool pressed) {
  encoder8_event_t e = {input, turn, pressed};
  xQueueSend(encoder8_events, &e, 0);  // a full queue drops it: the UI's that far behind anyway
}

void poll_task(void *) {
  reading last, now;
  bool have_last = false;  // last holds a reading to compare with
  int failing = 0;         // polls running it's not answered
  int part_click[encoder8_knobs] = {};

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(poll_ms));
    write_leds();
    if (!read_all(now)) {
      // Now and then it misses one, and its counts carry on. Unplugged for a while, they've started again
      if (++failing == lost_polls) {
        ESP_LOGW(TAG, "not answering");
        have_last = false;
        memset(part_click, 0, sizeof(part_click));
      }
      bus_recover();
      continue;
    }
    if (failing >= lost_polls) ESP_LOGI(TAG, "answering again");
    failing = 0;

    for (int i = 0; have_last && i < encoder8_knobs; i++) {
      int8_t counts = now.counter[i] - last.counter[i];
      int turn = (counts + part_click[i]) / counts_per_click;  // the rest waits for the next poll
      part_click[i] = counts + part_click[i] - turn * counts_per_click;
      if (turn) send(i, turn, false);
      if (now.button[i] != last.button[i]) send(i, 0, now.button[i] == 0);
    }
    if (have_last && now.sw != last.sw) send(encoder8_switch, 0, now.sw != 0);
    last = now;
    have_last = true;
  }
}

}  // namespace

QueueHandle_t encoder8_events = nullptr;

bool encoder8_begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint8_t address) {
  sda = sda_pin;
  scl = scl_pin;
  addr = address;
  for (gpio_num_t g : {sda, scl}) {
    gpio_reset_pin(g);
    gpio_set_level(g, 1);
    gpio_set_direction(g, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(g, GPIO_PULLUP_ONLY);  // the unit has its own as well
  }
  bus_recover();

  // It doesn't answer straight after it's powered up
  uint8_t firmware;
  bool found = false;
  for (int i = 0; i < 20 && !found; i++) {
    found = read_reg(REG_FIRMWARE, &firmware);
    if (!found) vTaskDelay(pdMS_TO_TICKS(50));
  }
  if (!found) {
    ESP_LOGI(TAG, "none found");
    return false;
  }
  ESP_LOGI(TAG, "found, firmware %d", firmware);

  encoder8_events = xQueueCreate(32, sizeof(encoder8_event_t));
  xTaskCreate(poll_task, "encoder8", 3072, nullptr, 1, nullptr);
  return true;
}

void encoder8_set_led(int led, uint8_t r, uint8_t g, uint8_t b) {
  if (led < 0 || led >= 9) return;
  taskENTER_CRITICAL(&leds_mux);
  if (leds[led][0] != r || leds[led][1] != g || leds[led][2] != b) {  // it's sent only if it's changed
    leds[led][0] = r;
    leds[led][1] = g;
    leds[led][2] = b;
    leds_dirty |= 1u << led;
  }
  taskEXIT_CRITICAL(&leds_mux);
}

void encoder8_dim_leds(bool dimmed) {
  taskENTER_CRITICAL(&leds_mux);
  leds_dimmed = dimmed;
  leds_dirty = (1u << 9) - 1;
  taskEXIT_CRITICAL(&leds_mux);
}
