#include <M5Unified.h>
#include <lvgl.h>

#include "pulsemote-pcb.hpp"

#if CONFIG_PULSEMOTE_PCB

#include "Rotary.h"
#include "rotaryencoders.hpp"

#include "pca9685.hpp"
#include "mcp23017.hpp"

MCP23017 mcp;
PCA9685 pca;

static constexpr int LED_COUNT = 6; // 4 switches + cherry
static lv_color_t g_led[LED_COUNT];
static uint32_t g_dirty = 0;
static portMUX_TYPE g_led_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t rotaryTask = nullptr;
static constexpr uint32_t NOTIF_LED = 1u << 0;
static constexpr uint32_t NOTIF_MCP = 1u << 1;

void RotaryEncoderChanged(bool clockwise, int id);

// MCP23017 is port expander on I2C x021 and INT on pin 6/7 (different if not
// CoreS3)
//
// b0 is sw1_rota b1 is sw1_rotb, b2 = sw1_button
// b3 is sw2_rota b4 is sw2_rotb, b5 = sw2_button
// b6 = cherry button
// a0 is sw4_button, a1 is sw4_rotb, a2 = sw4_rota
// a3 is sw3_rota, a4 is sw3_rotb, a5 = sw3_button

#if defined (CONFIG_IDF_TARGET_ESP32S3)
constexpr uint8_t INTA = 6;
constexpr uint8_t INTB = 7;
#else
constexpr uint8_t INTA = 27;
constexpr uint8_t INTB = 19;
#endif
const uint8_t buttonpins[] = {10, 13, 5, 0, 14};
const uint8_t numbuttons = sizeof(buttonpins);

QueueHandle_t event_queue = nullptr;

RotaryEncOverMCP rotaryEncoders[] = {
    RotaryEncOverMCP(&mcp, 9, 8, &RotaryEncoderChanged, 0),
    RotaryEncOverMCP(&mcp, 11, 12, &RotaryEncoderChanged, 1),
    RotaryEncOverMCP(&mcp, 4, 3, &RotaryEncoderChanged, 2),
    RotaryEncOverMCP(&mcp, 1, 2, &RotaryEncoderChanged, 3)};

// PWM controller is PCA9685PW on 0x42
//
// LED0 is R on SW1, LED1 is G on SW1, LED2 is B on SW1
// LED3 is R on SW2, LED4 is G on SW2, LED5 is B on SW2
// LED11 is R on SW3, LED12 is G on SW3, LED13 is B on SW3
// LED8 is R on SW4, LED9 is G on SW4, LED10 is B on SW4
// LED6 is Cherry LED

//PCA9685 PCA(0x42);

// Switch number 1-4 (5 for cherry, single LED), and lv_color_t

void pulsemote_pcb_setleds(uint8_t sw, lv_color_t rgb) {
  sw++; // so it's 1...5
  if (sw < 1 || sw > LED_COUNT) return;

  taskENTER_CRITICAL(&g_led_mux);
  g_led[sw - 1] = rgb;
  g_dirty |= (1u << (sw - 1));
  taskEXIT_CRITICAL(&g_led_mux);

  if (rotaryTask) {
    xTaskNotify(rotaryTask, NOTIF_LED, eSetBits);
  }
}

bool lastbuttonstates[numbuttons] = {false};
static unsigned long lastbuttondebounce[numbuttons] = {0};

void handlemcpinterrupt() {
  auto data = mcp.getCapturedInterrupt();
  //ESP_LOGE("MCP23017", "Interrupt data: 0x%04X", data);

  // check for rotary encoder change
  for (int i = 0; i < numencoders; i++) {
    rotaryEncoders[i].feedInput(data);
  }
  // check for button change
  unsigned long now = millis();
  for (uint8_t i = 0; i < numbuttons; i++) {
    if (now - lastbuttondebounce[i] > 150) {  // debounce time
      uint8_t result = (data >> buttonpins[i]) & 1;
      if (buttonpins[i] == 14) result = !result;  // cherry is inverted
      if (result != lastbuttonstates[i]) {
        event_t new_event = {.target = i, .value = result};
        xQueueSend(event_queue, &new_event, 0);  // no blocking, don't care if full
        lastbuttondebounce[i] = now;
      }
    }
  }
  mcp.clearInterrupts();
}

// Interrupt from MCP means a button or rotary encoder changed

static void IRAM_ATTR intactive(void* arg) {
  //ESP_LOGE("MCP23017", "GPIO interrupt");
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(rotaryTask, NOTIF_MCP, eSetBits, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void rotaryReaderTask(void* pArgs) {
  (void)pArgs;

  rotaryTask = xTaskGetCurrentTaskHandle();

  // Configure INTA/INTB as input with pullup + falling-edge interrupt
  gpio_config_t io{};
  io.pin_bit_mask = (1ULL << INTA) | (1ULL << INTB);
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_NEGEDGE;   // FALLING
  ESP_ERROR_CHECK(gpio_config(&io));

  esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_ERROR_CHECK(err);
  }

  ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)INTA, intactive, nullptr));
  ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)INTB, intactive, nullptr));

  mcp.readGPIOA(); // no interrupts unless you do a mcp.readGPIOA();
  mcp.readGPIOB();

  static const uint8_t pinstarts[LED_COUNT] = {0, 3, 11, 8, 6, 0}; // adjust last if needed

  while (true) {
    uint32_t bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &bits, pdMS_TO_TICKS(10));
    if (bits & NOTIF_MCP) {
      handlemcpinterrupt();
    }
    if (bits & NOTIF_LED) {
      uint32_t mask;

      taskENTER_CRITICAL(&g_led_mux);
      mask = g_dirty;
      g_dirty = 0;
      taskEXIT_CRITICAL(&g_led_mux);

      for (int idx = 0; idx < LED_COUNT; idx++) {
        if (!(mask & (1u << idx))) continue;

        lv_color_t rgb;
        taskENTER_CRITICAL(&g_led_mux);
        rgb = g_led[idx];
        taskEXIT_CRITICAL(&g_led_mux);

        uint8_t sw = idx + 1;
        uint8_t base = pinstarts[idx];

        pca.setPWM(base + 0, (uint16_t)rgb.red * 16);
        if (sw != 5) {
          pca.setPWM(base + 1, (uint16_t)rgb.green * 16);
          pca.setPWM(base + 2, (uint16_t)rgb.blue * 16);
        }
      }
    }
  }
}


void pulsemote_pcb_init(void) {
  auto& i2c = M5.Ex_I2C;
  i2c.begin();

  event_queue = xQueueCreate(10, sizeof(event_t));

  pca.attach(i2c, 0x42);
  if (!pca.begin(true)) { // true = invert outputs (MODE2 INVRT)
    ESP_LOGE("PCA9685", "No PCA9685 found");
    return;
  }
  mcp.attach(i2c, 0x21);
  if (!mcp.begin()) {
    ESP_LOGE("MCP23017", "No MCP23017 found");
    return;
  }
  if (!mcp.setupInterrupts(false, true, HIGH)) {
    ESP_LOGE("MCP23017", "Failed to setup interrupts");
    return;
  }

  for (uint8_t i = 0; i <= 15; ++i) {
    if (i == 0 || i == 5 || i == 10 || i == 13)
      mcp.pinMode(i, mcp.XINPUT);
    else
      mcp.pinMode(i, mcp.XINPUT_PULLUP);
  }
  for (uint8_t pin = 0; pin < 16; pin++) mcp.setupInterruptPin(pin, CHANGE);

  for (uint8_t i = 0; i < numencoders; i++)
    rotaryEncoders[i].init();  // currently a NOP

  xTaskCreatePinnedToCore(rotaryReaderTask, "io", 2048, nullptr, 20, nullptr, 1); // gui core
}

#else
void pulsemote_pcb_setleds(uint8_t sw, lv_color_t rgb) {};
void pulsemote_pcb_init(void) {};
QueueHandle_t event_queue = nullptr;
#endif
