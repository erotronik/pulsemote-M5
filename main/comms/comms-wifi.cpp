#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "config.h"
#include "comms-wifi.hpp"
#include "tab-mqtt.hpp"

#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>

// Wifi Comms via coprocessor esp-at connected to serial

QueueHandle_t mqttsubhandle;

#ifdef CONFIG_HASWIFICOPRO
#ifdef ARDUINO
HardwareSerial espat_copro_port(1);
#else
#include "driver/uart.h"
static const uart_port_t ESPAT_UART = UART_NUM_1;
static const int ESPAT_RX_BUF = 2048;   // ring buffer size (tune if needed)
static const int ESPAT_TX_BUF = 0;      // 0 = no TX ring buffer (direct writ
#endif
bool wifi_connected = false;
SemaphoreHandle_t atBusy;
EventGroupHandle_t responseFlags;
static const int RESPONSE_OK    = (1 << 0);
static const int RESPONSE_ERROR = (1 << 1);

int cstate = 0;
unsigned long cstate_timeout;

QueueHandle_t mqttsenthandle;

static esp_err_t espat_uart_init(void) {
    const uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(ESPAT_UART, ESPAT_RX_BUF, ESPAT_TX_BUF, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(ESPAT_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(ESPAT_UART, /*tx*/ 17, /*rx*/ 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    return ESP_OK;
}

static inline bool streq(const char* a, const char* b) {
  return std::strcmp(a, b) == 0;
}

static inline bool starts_with(const char* s, const char* prefix) {
  return std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

static inline void trim_inplace(char* s) {
  // trim leading
  char* p = s;
  while (*p && std::isspace((unsigned char)*p)) p++;
  if (p != s) std::memmove(s, p, std::strlen(p) + 1);

  // trim trailing
  size_t n = std::strlen(s);
  while (n > 0 && std::isspace((unsigned char)s[n - 1])) {
    s[n - 1] = '\0';
    n--;
  }
}

static inline int to_int_arduinoish(const char* s) {
  // Roughly matches Arduino String::toInt(): returns 0 if no leading number
  char* end = nullptr;
  long v = std::strtol(s, &end, 10);
  if (end == s) return 0;
  return (int)v;
}

void mqttsend(const char *topic, const char *message) {
  mqttsenditem item;
  std::memset(&item, 0, sizeof(item));
  std::strncpy(item.topic, topic, max_topic_size - 1);
  std::strncpy(item.message, message, max_message_size - 1);
  item.command = 0;
  xQueueSend(mqttsenthandle, &item, 0);
}

void mqttsubscribe(const char *topic) {
  mqttsenditem item;
  std::memset(&item, 0, sizeof(item));
  std::strncpy(item.topic, topic, max_topic_size - 1);
  item.command = 1;
  xQueueSend(mqttsenthandle, &item, 0);
}

bool is_wifi_connected() {
  return wifi_connected;
}

bool wifi_loop() {
  return false;
}

// Parse +MQTTSUBRECV line and enqueue mqttsenditem
static void handle_mqttsubrecv(const char* line) {
  // Example: +MQTTSUBRECV:0,"wled/red/status",7,offline
  const char* q1 = std::strchr(line, '"');
  if (!q1) return;
  const char* q2 = std::strchr(q1 + 1, '"');
  if (!q2 || q2 <= q1 + 1) return;

  // Extract topic
  char topic[max_topic_size];
  const size_t topic_len = (size_t)(q2 - (q1 + 1));
  if (topic_len >= max_topic_size) return;
  std::memcpy(topic, q1 + 1, topic_len);
  topic[topic_len] = '\0';

  // Find comma after closing quote
  const char* comma_after_topic = std::strchr(q2 + 1, ',');
  if (!comma_after_topic) return;

  // Find comma after length
  const char* comma_after_len = std::strchr(comma_after_topic + 1, ',');
  if (!comma_after_len) return;

  // Parse length substring
  char lenStr[16];
  size_t len_len = (size_t)(comma_after_len - (comma_after_topic + 1));
  if (len_len >= sizeof(lenStr)) len_len = sizeof(lenStr) - 1;
  std::memcpy(lenStr, comma_after_topic + 1, len_len);
  lenStr[len_len] = '\0';
  trim_inplace(lenStr);
  int payload_len = to_int_arduinoish(lenStr);

  // Payload starts after comma_after_len
  const char* data_start = comma_after_len + 1;
  if (!*data_start) return;

  // Determine available bytes (up to end of line)
  const int available = (int)std::strlen(data_start);
  int take = available;
  if (payload_len > 0 && payload_len <= available) take = payload_len;

  // Extract message
  char message[max_message_size];
  if (take >= max_message_size) take = max_message_size - 1;
  std::memcpy(message, data_start, (size_t)take);
  message[take] = '\0';

  // Trim trailing CR/LF
  size_t mlen = std::strlen(message);
  while (mlen > 0 && (message[mlen - 1] == '\r' || message[mlen - 1] == '\n')) {
    message[mlen - 1] = '\0';
    mlen--;
  }

  mqttsenditem item;
  std::memset(&item, 0, sizeof(item));
  std::snprintf(item.topic,   max_topic_size,   "%s", topic);
  std::snprintf(item.message, max_message_size, "%s", message);

  ESP_LOGD("mqtt rx", "%s=%s", item.topic, item.message);
  xQueueSend(mqttsubhandle, &item, 0);
}

static void espat_handleLine(const char* line) {
  if (streq(line, "WIFI DISCONNECT") && wifi_connected) {
    wifi_connected = false;
    cstate = 0;
  }

  if (starts_with(line, "+MQTTDISCONNECTED") && wifi_connected) {
    cstate = 8;
    wifi_connected = false;
    return;
  }

  if (cstate == 9 && starts_with(line, "+MQTTCONNECTED")) {
    cstate++;
    return;
  }

  if (cstate == 1 && streq(line, "ready")) {
    cstate++;
    return;
  }

  if (cstate == 5 && streq(line, "WIFI GOT IP")) {
    cstate++;
    return;
  }

  if (streq(line, "OK")) {
    if (cstate == 3 || cstate == 7 || cstate == 11) {
      cstate++;
      return;
    }
    xEventGroupSetBits(responseFlags, RESPONSE_OK);
    return;
  }

  if (streq(line, "ERROR")) {
    xEventGroupSetBits(responseFlags, RESPONSE_ERROR);
    return;
  }

  if (starts_with(line, "+MQTTSUBRECV")) {
    handle_mqttsubrecv(line);
  } else {
    //ESP_LOGD("[ASYNC]", "%s", line);
  }
}

static void espat_processSerialInput(void) {
    static char espat_buffer[512];
    static size_t espat_len = 0;

    size_t avail = 0;
    if (uart_get_buffered_data_len(ESPAT_UART, &avail) != ESP_OK || avail == 0) {
        return;
    }

    uint8_t tmp[128];
    while (avail > 0) {
        const int to_read = (avail > sizeof(tmp)) ? sizeof(tmp) : (int)avail;
        const int n = uart_read_bytes(ESPAT_UART, tmp, to_read, 0 /* no wait */);
        if (n <= 0) break;

        for (int i = 0; i < n; i++) {
            char c = (char)tmp[i];
            if (c == '\r') continue;

            if (c == '\n') {
                if (espat_len > 0) {
                    espat_buffer[espat_len] = '\0';
                    ESP_LOGI("espat", "rx=%s", espat_buffer);
                    espat_handleLine(espat_buffer);
                    espat_len = 0;
                }
            } else {
                if (espat_len < sizeof(espat_buffer) - 1) {
                    espat_buffer[espat_len++] = c;
                } else {
                    // overflow: drop line
                    espat_len = 0;
                }
            }
        }

        uart_get_buffered_data_len(ESPAT_UART, &avail);
    }
}

static bool espat_sendATCommand(const char* cmd, uint32_t timeout = 6000) {
  if (xSemaphoreTake(atBusy, portMAX_DELAY) != pdTRUE) return false;

  xEventGroupClearBits(responseFlags, RESPONSE_OK | RESPONSE_ERROR);
  ESP_LOGI("espat", "txwait=%s", cmd);

  uart_write_bytes(ESPAT_UART, cmd, (int)strlen(cmd));
  uart_write_bytes(ESPAT_UART, "\r\n", 2);

  const unsigned long start = millis();
  while (millis() - start < timeout) {
    espat_processSerialInput();

    const EventBits_t bits = xEventGroupGetBits(responseFlags);
    if (bits & RESPONSE_OK) {
      xEventGroupClearBits(responseFlags, RESPONSE_OK);
      xSemaphoreGive(atBusy);
      return true;
    }
    if (bits & RESPONSE_ERROR) {
      xEventGroupClearBits(responseFlags, RESPONSE_ERROR);
      xSemaphoreGive(atBusy);
      return false;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  xSemaphoreGive(atBusy);
  ESP_LOGD("espat", "timeout");
  return false;
}

static void espat_sendAT(const char* cmd) {
  ESP_LOGI("espat", "tx=%s", cmd);
  uart_write_bytes(ESPAT_UART, cmd, (int)strlen(cmd));
  uart_write_bytes(ESPAT_UART, "\r\n", 2);
}

void wifi_task(void* pvParameters) {
  (void)pvParameters;
  ESP_LOGD("espat","starting up");

  mqttsenditem mqtttosend;
  wifi_connected = false;
  cstate = 0;

  atBusy = xSemaphoreCreateMutex();
  responseFlags = xEventGroupCreate();

  // cores3 pc_tx (g17) pc_rx (g18); bus_pc_rx/tx
  espat_uart_init();
  vTaskDelay(500 / portTICK_PERIOD_MS);

  while (true) {
    if (cstate_timeout != 0 && millis() > cstate_timeout + 20000) {
      cstate = 0;
      ESP_LOGD("espat","timeout will retry");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

#ifdef CONFIG_WIFI_SSID
    if (cstate == 0) {
      espat_sendAT("AT+RST");
      cstate++;
      cstate_timeout = millis();
    }
    if (cstate == 2) {
      espat_sendAT("AT+CWMODE=1");
      cstate++;
    }
    if (cstate == 4) {
      // AT+CWJAP="<ssid>","<pass>"
      char cmd[256];
      std::snprintf(cmd, sizeof(cmd),
                    "AT+CWJAP=\"" CONFIG_WIFI_SSID "\",\"" CONFIG_WIFI_PASSWORD "\"");
      espat_sendAT(cmd);
      cstate++;
    }
    if (cstate == 6) {
      char cmd[256];
      std::snprintf(cmd, sizeof(cmd),
                    "AT+MQTTUSERCFG=0,1,\"esp32\",\"" CONFIG_MQTT_USERNAME "\",\""
                    CONFIG_MQTT_PASSWORD "\",0,0,\"\"");
      espat_sendAT(cmd);
      cstate++;
    }
    if (cstate == 8) {
      char cmd[256];
      std::snprintf(cmd, sizeof(cmd),
                    "AT+MQTTCONN=0,\"" CONFIG_MQTT_SERVER "\",1883,0");
      espat_sendAT(cmd);
      cstate++;
    }
#endif

    if (cstate == 10) {
      espat_sendAT("ATE0");
      cstate++;
    }
    if (cstate == 12) {
      wifi_connected = true;
      cstate_timeout = 0;
      cstate++;
    }

    espat_processSerialInput();

    if (wifi_connected) {
      while (xQueueReceive(mqttsenthandle, &mqtttosend, 0)) {
        ESP_LOGD("mqtt", "handling item with topic %s", mqtttosend.topic);

        if (mqtttosend.command == 0) {
          // Publish: AT+MQTTPUB=0,"<topic>","<message>",1,0
          char cmd[512];
          std::snprintf(cmd, sizeof(cmd),
                        "AT+MQTTPUB=0,\"%s\",\"%s\",1,0",
                        mqtttosend.topic, mqtttosend.message);
          espat_sendATCommand(cmd);
        } else if (mqtttosend.command == 1) {
          // Subscribe: AT+MQTTSUB=0,"<topic>",0
          char cmd[256];
          std::snprintf(cmd, sizeof(cmd),
                        "AT+MQTTSUB=0,\"%s\",0",
                        mqtttosend.topic);
          espat_sendAT(cmd);
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void wifi_setup() {
  mqttsenthandle = xQueueCreate(10, sizeof(mqttsenditem));
  mqttsubhandle  = xQueueCreate(10, sizeof(mqttsenditem));
  wifi_connected = false;
  xTaskCreatePinnedToCore(wifi_task, "wifi", 1024 * 12, NULL, 1, nullptr, 0); // run wifi also on core0
}

#else
#if NOTWORKING
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

bool wifi_connected = false;
unsigned long cstate_timeout;

QueueHandle_t mqttsenthandle;

static EventGroupHandle_t s_wifi_event_group;
static constexpr int WIFI_CONNECTED_BIT = BIT0;
static constexpr int WIFI_FAIL_BIT      = BIT1;
static int s_retry_num = 0;

static void wifi_event_handler(void*,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 10) {
      s_retry_num++;
      esp_wifi_connect();
      ESP_LOGI("wifi", "retry to connect (%d/10)", s_retry_num);
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    s_retry_num = 0;
    ESP_LOGI("wifi", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_task(void* pvParameters) {
  (void)pvParameters;
  ESP_LOGI("wifi ","starting up");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

  wifi_config_t wifi_config{};
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid));
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password), CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group,
      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
      pdFALSE,
      pdFALSE,
      pdMS_TO_TICKS(15000));
}

void wifi_setup() {   
  mqttsenthandle = xQueueCreate(10, sizeof(mqttsenditem));
  mqttsubhandle  = xQueueCreate(10, sizeof(mqttsenditem));
  //wifi_task(nullptr);
  wifi_connected = false;xTaskCreatePinnedToCore(wifi_task, "wifi", 1024 * 12, NULL, 1, nullptr, 1); // run wifi also on core0
}

bool wifi_loop() { return false; }
bool is_wifi_connected() { return wifi_connected; }

void mqttsend(const char *topic, const char *message) {}
void mqttsubscribe(const char *topic) {}

#else
void wifi_setup() { mqttsubhandle  = xQueueCreate(10, sizeof(mqttsenditem));}
bool wifi_loop() { return false; }
bool is_wifi_connected() { return false; }
void mqttsend(const char *topic, const char *message) {}
void mqttsubscribe(const char *topic) {}
#endif
#endif
