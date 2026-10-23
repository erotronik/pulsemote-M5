#pragma once

bool is_wifi_connected(void);
void wifi_setup(void);
void mqttsend(const char *topic, const char *message, bool retain = false);
int mqttsend_space(void);  // how many more messages mqttsend() can queue just now
void mqttsubscribe(const char *topic);
bool wifi_loop(void);


static const int max_topic_size = 100;
static const int max_message_size = 640;  // enough for a Home Assistant discovery message

typedef struct {
      int command;
      bool retain;
      char topic[max_topic_size];
      char message[max_message_size];
} mqttsenditem;

extern QueueHandle_t mqttsubhandle;