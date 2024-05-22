#pragma once

#include <WiFi.h>
#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "config.h"

#include <PubSubClient.h>

class tab_mqtt;

class tab_mqtt : public Tab {
  public:
    tab_mqtt();
    void mqttsend(const char *topic, const char *message);
    void loop(boolean activetab) override;
    const char *geticons(void) override;
    const char* gettabname(void) override;
    void setup(void) override;
    void gotsyncdata(Tab *t, sync_data status) override;

  private:
    static void wifiTask(void* pvParameters);
    void connectToWiFi(void);
    void callback(char* topic, byte* payload, unsigned int length);

    WiFiClient espClient;
    PubSubClient *client = nullptr;
    QueueHandle_t events;

    typedef struct {
      const char *topic;
      const char *message;
    } mqttsenditem;

    QueueHandle_t mqttsenthandle;
};

