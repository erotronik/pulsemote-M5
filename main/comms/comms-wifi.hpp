#pragma once

bool is_wifi_connected(void);
void wifi_setup(void);
void mqttsend(const char *topic, const char *message);
void mqttsubscribe(const char *topic);
bool wifi_loop(void);


static const int max_topic_size = 100;
static const int max_message_size = 256;

typedef struct {
      int command;
      char topic[max_topic_size];
      char message[max_message_size];
} mqttsenditem;

extern QueueHandle_t mqttsubhandle;