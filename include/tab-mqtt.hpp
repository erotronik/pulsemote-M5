#pragma once

#include <WiFi.h>
#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "config.h"

#include <PubSubClient.h>

class tab_mqtt;

void ok_event_cb(lv_event_t * e);
void close_event_cb(lv_event_t * e);
void list_event_handler(lv_event_t * e);

class tab_mqtt : public Tab {
  public:
    tab_mqtt();
    void mqttsend(const char *topic, const char *message);
    void loop(boolean activetab) override;
    const char *geticons(void) override;
    const char* gettabname(void) override;
    void setup(void) override;
    void gotsyncdata(Tab *t, sync_data status) override;
    void popup_add_device(lv_obj_t *base);

  private:
    static void popup_add_device_ok_event_cb(lv_event_t * e);
    static void popup_add_device_list_event_handler(lv_event_t * e);
    static void popup_add_device_close_event_cb(lv_event_t * e);
    bool popup_add_device_open = false;
    lv_obj_t * selected_btn;
    lv_style_t style_selected;
    lv_obj_t * popup_add_device_modal;

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

