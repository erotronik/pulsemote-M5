#pragma once

#include "lvgl-utils.h"
#include <M5Unified.h>

#ifdef M5_BOARD
//#include "PCA9685.h"
#include "Rotary.h"
#include "hardware-RotaryEncOverMCP.h"
#endif

#include "i2c_bus_m5.h"
#include "hardware-pca9685-min.h"
//static I2CBusM5 i2c(M5.Ex_I2C);
static PCA9685 pca(M5.Ex_I2C, 0x42);

typedef struct {
  int target;
  int value;
} event_t;

typedef uint8_t byte;

QueueHandle_t event_queue;

#ifdef M5_BOARD

static constexpr int LED_COUNT = 6; // 4 switches + cherry
static lv_color_t g_led[LED_COUNT];
static uint32_t g_dirty = 0;
static portMUX_TYPE g_led_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t g_led_task = nullptr;
static constexpr uint32_t NOTIF_LED = 1u << 0;

void RotaryEncoderChanged(bool clockwise, int id);

// MCP23017 is port expander on I2C x021 and INT on pin 6/7 (different if not
// CoreS3)
//
// b0 is sw1_rota b1 is sw1_rotb, b2 = sw1_button
// b3 is sw2_rota b4 is sw2_rotb, b5 = sw2_button
// b6 = cherry button
// a0 is sw4_button, a1 is sw4_rotb, a2 = sw4_rota
// a3 is sw3_rota, a4 is sw3_rotb, a5 = sw3_button

Adafruit_MCP23X17 mcp;

#if defined (CONFIG_IDF_TARGET_ESP32S3)
constexpr uint8_t INTA = 6;
constexpr uint8_t INTB = 7;
#else
constexpr uint8_t INTA = 27;
constexpr uint8_t INTB = 19;
#endif
const byte buttonpins[] = {10, 13, 5, 0, 14};
const byte numbuttons = sizeof(buttonpins);

static TaskHandle_t rotaryTask = nullptr;

RotaryEncOverMCP rotaryEncoders[] = {
    RotaryEncOverMCP(&mcp, 9, 8, &RotaryEncoderChanged, 0),
    RotaryEncOverMCP(&mcp, 11, 12, &RotaryEncoderChanged, 1),
    RotaryEncOverMCP(&mcp, 4, 3, &RotaryEncoderChanged, 2),
    RotaryEncOverMCP(&mcp, 1, 2, &RotaryEncoderChanged, 3)};
const byte numencoders = 4;

// PWM controller is PCA9685PW on 0x42
//
// LED0 is R on SW1, LED1 is G on SW1, LED2 is B on SW1
// LED3 is R on SW2, LED4 is G on SW2, LED5 is B on SW2
// LED11 is R on SW3, LED12 is G on SW3, LED13 is B on SW3
// LED8 is R on SW4, LED9 is G on SW4, LED10 is B on SW4
// LED6 is Cherry LED

//PCA9685 PCA(0x42);

// Switch number 1-4 (5 for cherry, single LED), and lv_color_t

void m5io_showanalogrgb(byte sw, lv_color_t rgb) {
  if (sw < 1 || sw > LED_COUNT) return;

  taskENTER_CRITICAL(&g_led_mux);
  g_led[sw - 1] = rgb;
  g_dirty |= (1u << (sw - 1));
  taskEXIT_CRITICAL(&g_led_mux);

  if (g_led_task) {
    xTaskNotify(g_led_task, NOTIF_LED, eSetBits);
  }
}

bool lastbuttonstates[numbuttons] = {false};
static unsigned long lastbuttondebounce[numbuttons] = {0};

void handlemcpinterrupt() {
  auto data = mcp.getCapturedInterrupt();

  // check for rotary encoder change
  for (int i = 0; i < numencoders; i++) {
    rotaryEncoders[i].feedInput(data);
  }
  // check for button change
  unsigned long now = millis();
  for (byte i = 0; i < numbuttons; i++) {
    if (now - lastbuttondebounce[i] > 150) {  // debounce time
      byte result = (data >> buttonpins[i]) & 1;
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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(rotaryTask, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void rotaryReaderTask(void* pArgs) {
  (void)pArgs;

  rotaryTask = xTaskGetCurrentTaskHandle();
  g_led_task = xTaskGetCurrentTaskHandle();

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

  static const byte pinstarts[LED_COUNT] = {0, 3, 11, 8, 6, 0}; // adjust last if needed

  while (true) {
    uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
    if (n > 0) {
      handlemcpinterrupt();
    }
    uint32_t bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &bits,0);
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

        byte sw = idx + 1;
        uint8_t base = pinstarts[idx];

        //PCA.setPWM(base + 0, rgb.red * 16);
        pca.setPWM(base + 0, (uint16_t)rgb.red * 16);
        if (sw != 5) {
          pca.setPWM(base + 1, (uint16_t)rgb.green * 16);
          pca.setPWM(base + 2, (uint16_t)rgb.blue * 16);
          //PCA.setPWM(base + 1, rgb.green * 16);
          //PCA.setPWM(base + 2, rgb.blue * 16);
        }
      }
    }
  }
}
        //pca.setPWM(base + 0, (uint16_t)rgb.red * 16)
          //pca.setPWM(base + 1, (uint16_t)rgb.green * 16);
          //pca.setPWM(base + 2, (uint16_t)rgb.blue * 16); 

void m5io_init(void) {
  event_queue = xQueueCreate(10, sizeof(event_t));

  M5.Ex_I2C.begin();

  //if (!PCA.begin(PCA9685_MODE1_AUTOINCR | PCA9685_MODE1_ALLCALL, PCA9685_MODE2_INVERT)) {
  if (!pca.begin(true)) { // true = invert outputs (MODE2 INVRT)
    printf_log("No PCA9685 found");
  }

  delay(250);

  if (!mcp.begin_I2C(0x21)) {
    printf_log("No MCP23X17 found");
    return;
  }

  mcp.setupInterrupts(false, true, HIGH);

  for (byte i = 0; i <= 15; ++i) {
    if (i == 0 || i == 5 || i == 10 || i == 13)
      mcp.pinMode(i, INPUT);
    else
      mcp.pinMode(i, INPUT_PULLUP);
  }
  for (byte pin = 0; pin < 16; pin++) mcp.setupInterruptPin(pin, CHANGE);

  for (byte i = 0; i < numencoders; i++)
    rotaryEncoders[i].init();  // currently a NOP

  xTaskCreatePinnedToCore(rotaryReaderTask, "io", 2048, nullptr, 20, nullptr, 1); // gui core

}
#else
  // Waveshare has no IO (yet) but can't use the same M5 libraries anyway due to i2c driver conflicts
  const byte numencoders = 4;
  void m5io_init(void) {
      event_queue = xQueueCreate(10, sizeof(event_t));
  }
  void m5io_showanalogrgb(byte sw, lv_color_t rgb) {
  }

#endif
