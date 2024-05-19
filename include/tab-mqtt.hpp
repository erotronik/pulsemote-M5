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
    WiFiClient espClient;
    PubSubClient *client;

    const char* gettabname(void) override { return "(WiFi)";};

    static void wifiTask(void* pvParameters) {
      ESP_LOGD("wifitask","starting up");
    
      tab_mqtt* self = static_cast<tab_mqtt *>(pvParameters);
      self->connectToWiFi();

#ifdef CONFIG_MQTT_SERVER
      self->client = new PubSubClient(self->espClient);
      self->client->setServer(CONFIG_MQTT_SERVER,1883);
      self->client->setCallback(callback);

      while (true) {
        if (!self->client->connected()) {
          while (!self->client->connected()) {
            ESP_LOGI("wifi","Attempting MQTT connection...");
            if (self->client->connect("pulsemote",CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD)) {
              ESP_LOGI("wifi","wifi connected");
              self->client->publish("pulsemote/main","startup");
              self->client->subscribe("pulsemote/#");
            } else {
              ESP_LOGE("wifi","wifi failed, rc=%s retrying",self->client->state());
              vTaskDelay(5000/portTICK_PERIOD_MS);
            }
          }
        }
        self->client->loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
#endif
      vTaskDelete(NULL);
    };

    void setup(void) override {
      tab_mqtt *mt = static_cast<tab_mqtt *>(this);
      mt->type = DeviceType::device_mqtt;
      mt->device = nullptr;
      mt->last_change = mt->old_last_change = D_NONE;

#ifdef CONFIG_WIFI_SSID
      xTaskCreatePinnedToCore(wifiTask, "wifi", 1024 * 10, this, 1, nullptr, 1);
#endif
    };

    static void callback(char* topic, byte* payload, unsigned int length) {
      Serial.print("Message arrived [");
      Serial.print(topic);
      Serial.print("] ");
      for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
      }
      Serial.println();
    };

    void connectToWiFi(void) {
#ifdef CONFIG_WIFI_SSID
      ESP_LOGI("wifi","connecting to wifi");
      WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      ESP_LOGI("wifi","connected to wifi");
#endif
    };

    void gotsyncdata(Tab *t, sync_data status) override {
      ESP_LOGD("mqtt", "got sync data %d from %s\n", status, t->device->getShortName());
      if (!client || !client->connected()) return;
      String topic = "pulsemote/" + String(t->device->getShortName());
      topic.replace("-","");
      topic.toLowerCase();
      char outputChar[topic.length() + 1];
      topic.toCharArray(outputChar, topic.length() + 1);
      if (status == SYNC_ON) 
        client->publish(outputChar,"on");
      if (status == SYNC_OFF) 
        client->publish(outputChar,"off");
      if (status == SYNC_START) 
        client->publish(outputChar,"hello");
      if (status == SYNC_BYE) 
        client->publish(outputChar,"bye");  
    };
};

