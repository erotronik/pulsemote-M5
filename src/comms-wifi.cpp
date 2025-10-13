#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "config.h"
#include "tab-mqtt.hpp"
#include "tab-mqtt-socket.hpp"

#include <freertos/queue.h>
#include <freertos/task.h>

// Wifi Comms via coprocessor est-at connected to serial

HardwareSerial espat_copro_port(1);
bool wifi_connected = false;
SemaphoreHandle_t atBusy;
EventGroupHandle_t responseFlags;
static const int RESPONSE_OK    = (1 << 0);
static const int RESPONSE_ERROR = (1 << 1) ; 
int cstate = 0;
unsigned long cstate_timeout;

static const int max_topic_size = 100;
static const int max_message_size = 256;
typedef struct {
      int command;
      char topic[max_topic_size];
      char message[max_message_size];
} mqttsenditem;
QueueHandle_t mqttsenthandle, mqttsubhandle;

void mqttsend(const char *topic, const char *message) {
  mqttsenditem item;
  strncpy(item.topic, topic, max_topic_size-1);
  strncpy(item.message, message, max_message_size-1);
  item.command = 0;
  xQueueSend(mqttsenthandle, &item, 0);
}

void mqttsubscribe(const char *topic) {
  mqttsenditem item;
  strncpy(item.topic, topic, max_topic_size-1);
  item.command = 1;
  xQueueSend(mqttsenthandle, &item, 0);
}

bool is_wifi_connected() {
    return wifi_connected;
}

bool wifi_loop() {
    mqttsenditem item;

    while (xQueueReceive(mqttsubhandle, &item, 0)) {
        ESP_LOGD("wifi_loop","%s=%s",item.topic,item.message);
    }
    return false;
}

void espat_handleLine(const String& line) {
  if (line == "WIFI DISCONNECT" && wifi_connected) {
    wifi_connected = false;
    cstate = 0;
  }
  if (line.startsWith("+MQTTDISCONNECTED") && wifi_connected) {
    cstate = 8;
    wifi_connected = false;
    return;
  }
  if (cstate == 9 && line.startsWith("+MQTTCONNECTED")) {
    cstate++;
    return;
  }
  if (cstate == 1 && line == "ready") {
    cstate++;
    return;
  }
  if (cstate == 5 && line == "WIFI GOT IP") {
    cstate++;
    return;
  }
  if (line == "OK") {
    if (cstate == 3 || cstate == 7 || cstate == 11) {
      cstate++;
      return;
    }
    xEventGroupSetBits(responseFlags, RESPONSE_OK);
  } else if (line == "ERROR") {
    xEventGroupSetBits(responseFlags, RESPONSE_ERROR);
  } else {
    if (line.startsWith("+MQTTSUBRECV")) {
      int valueStart = 16;
      int valueEnd = line.indexOf("\"", valueStart);
      if (valueEnd > valueStart) {
        String topic = line.substring(valueStart,valueEnd);
      
        int stateIndex = line.indexOf(",O");
        if (stateIndex >= 0) {
          valueStart = stateIndex + 1;
          mqttsenditem item;
          String message = line.substring(valueStart);
          ESP_LOGD("mqtt rx","%s=%s",topic.c_str(),message.c_str());
          strncpy(item.topic, topic.c_str(), max_topic_size-1);
          strncpy(item.message, message.c_str(), max_message_size-1);
          xQueueSend(mqttsubhandle, &item, 0);
        }
      }
      //ESP_LOGD("[SUB] ","%s", line.c_str());
    } else {
      //ESP_LOGD("[ASYNC] ","%s", line.c_str());
    }
  }
}

void espat_processSerialInput() {
  static String espat_buffer = "";

  while (espat_copro_port.available()) {
    char c = espat_copro_port.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (espat_buffer.length() > 0) {
        ESP_LOGD("espat","rx=%s",espat_buffer.c_str());
        espat_handleLine(espat_buffer);
        espat_buffer = "";
      }
    } else {
      espat_buffer += c;
    }
  }
}

bool espat_sendATCommand(const String& cmd, uint32_t timeout = 6000) {
  if (xSemaphoreTake(atBusy, portMAX_DELAY) != pdTRUE) return false;

  xEventGroupClearBits(responseFlags, RESPONSE_OK | RESPONSE_ERROR);
  ESP_LOGD("espat","txwait=%s",cmd.c_str());
  espat_copro_port.print(cmd + "\r\n");
  //ESP_LOGD("mqtt wf response","");

  unsigned long start = millis();
  while (millis() - start < timeout) {
    espat_processSerialInput();  

    EventBits_t bits = xEventGroupGetBits(responseFlags);
    if (bits & RESPONSE_OK) {
      xEventGroupClearBits(responseFlags, RESPONSE_OK);
      xSemaphoreGive(atBusy);
      return true;
    } else if (bits & RESPONSE_ERROR) {
      xEventGroupClearBits(responseFlags, RESPONSE_ERROR);
      xSemaphoreGive(atBusy);
      return false;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);  // Yield to avoid WDT
  }
  xSemaphoreGive(atBusy);
  ESP_LOGD("espat","timeout");
  return false;
}

void espat_sendAT(const String& cmd) {
  ESP_LOGD("espat","tx=%s",cmd.c_str());
  espat_copro_port.print(cmd + "\r\n");
  return;
}

void wifi_task(void* pvParameters) {
  ESP_LOGD("espat","starting up");

  mqttsenditem mqtttosend;
  wifi_connected = false;
  cstate = 0;

  atBusy = xSemaphoreCreateMutex();
  responseFlags = xEventGroupCreate();

  // cores3 pc_tx (g17) pc_rx (g18); bus_pc_rx/tx
  espat_copro_port.begin(115200, SERIAL_8N1, 18, 17);
  //self->sendAT("AT+UART_DEF=115200,8,1,0,0"); if you want to run it slower
  vTaskDelay(500 / portTICK_PERIOD_MS);

  while (true) {
    if (cstate_timeout !=0 && millis() > cstate_timeout + 20000) {
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
      espat_sendAT("AT+CWJAP=\"" CONFIG_WIFI_SSID "\",\"" CONFIG_WIFI_PASSWORD "\"");
      cstate++;
    }
    if (cstate == 6) {
      espat_sendAT("AT+MQTTUSERCFG=0,1,\"esp32\",\"" CONFIG_MQTT_USERNAME "\",\"" CONFIG_MQTT_PASSWORD "\",0,0,\"\"");
      cstate++;
    }
    if (cstate == 8) {
      espat_sendAT("AT+MQTTCONN=0,\"" CONFIG_MQTT_SERVER "\",1883,0");
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
        if (mqtttosend.command == 0) {
            espat_sendATCommand("AT+MQTTPUB=0,\""+String(mqtttosend.topic)+"\",\""+String(mqtttosend.message)+"\",1,0");
        } else if (mqtttosend.command == 1) {
            espat_sendAT("AT+MQTTSUB=0,\""+String(mqtttosend.topic)+"\",0");
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void wifi_setup() {
    mqttsenthandle = xQueueCreate(10,sizeof(mqttsenditem));
    mqttsubhandle = xQueueCreate(10,sizeof(mqttsenditem));
    xTaskCreatePinnedToCore(wifi_task, "wifi", 1024 * 12, NULL, 1, nullptr, 0); // run wifi also on core0
}