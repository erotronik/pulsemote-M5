#include <WiFi.h>
#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "config.h"
#include "tab-mqtt.hpp"
#include <freertos/queue.h>
#include <freertos/task.h>

#include <PubSubClient.h>

tab_mqtt::tab_mqtt() {
  events = xQueueCreate(10,sizeof(sync_data));
  mqttsenthandle = xQueueCreate(10,sizeof(mqttsenditem));
}

void tab_mqtt::mqttsend(const char *topic, const char *message) {
  mqttsenditem item;
  item.topic = topic;
  item.message = message;
  xQueueSend(mqttsenthandle, &item, 0);
}

void tab_mqtt::loop(boolean activetab) {
  sync_data syncstatus;
  if (xQueueReceive(events, &syncstatus, 0))
    send_sync_data(syncstatus);
}

const char *tab_mqtt::geticons(void) {
  return (client->connected()?LV_SYMBOL_WIFI:"");
}

const char* tab_mqtt::gettabname(void){ 
  return "wifi";
}

void tab_mqtt::wifiTask(void* pvParameters) {
  ESP_LOGD("wifitask","starting up");
  mqttsenditem mqtttosend;
    
  tab_mqtt* self = static_cast<tab_mqtt *>(pvParameters);
  self->connectToWiFi();

#ifdef CONFIG_MQTT_SERVER
  self->client = new PubSubClient(self->espClient);
  self->client->setServer(CONFIG_MQTT_SERVER,1883);
  self->client->setCallback(std::bind(&tab_mqtt::callback, self, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3 ));

  while (true) {

    // does anything want us to send a message?
    if (self->client->connected() && xQueueReceive(self->mqttsenthandle, &mqtttosend, 0))
      self->client->publish(mqtttosend.topic,mqtttosend.message);

    if (!self->client->connected()) {
       while (!self->client->connected()) {
        ESP_LOGI("wifi","Attempting MQTT connection...");
        if (self->client->connect("pulsemote",CONFIG_MQTT_USERNAME, CONFIG_MQTT_PASSWORD)) {
          ESP_LOGI("wifi","wifi connected");
          self->client->publish("pulsemote/main","startup");
          self->client->subscribe("pulsemote/#");
#ifdef CONFIG_MQTT_TEST
          self->client->subscribe(CONFIG_MQTT_TEST);
#endif
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
}

void tab_mqtt::setup(void) {
  tab_mqtt *mt = static_cast<tab_mqtt *>(this);
  mt->type = DeviceType::device_mqtt;
  mt->device = nullptr;
  mt->last_change = mt->old_last_change = D_NONE;

#ifdef CONFIG_WIFI_SSID
  xTaskCreatePinnedToCore(wifiTask, "wifi", 1024 * 10, this, 1, nullptr, 1);
#endif
}

void tab_mqtt::callback(char* topic, byte* payload, unsigned int length) {
  sync_data syncstatus = SYNC_START;
  std::string paystring (reinterpret_cast<const char*>(payload), length); 
  ESP_LOGI("mqtt","got topic=%s message=%s", topic, paystring.c_str());
  if (!strncmp(topic,"pulsemote",9)) {
    if (paystring == "ON" || paystring == "on") syncstatus = SYNC_ON;
    if (paystring == "OFF" || paystring == "off") syncstatus = SYNC_OFF;
    xQueueSend(this->events, &syncstatus, 0);
  }
}

void tab_mqtt::connectToWiFi(void) {
#ifdef CONFIG_WIFI_SSID
  ESP_LOGI("wifi","connecting to wifi");
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  ESP_LOGI("wifi","connected to wifi");
#endif
}

void tab_mqtt::gotsyncdata(Tab *t, sync_data status) {
  ESP_LOGD("mqtt", "got sync data %d from %s\n", status, t->gettabname());
  if (!client || !client->connected()) return;
  String topic = "pulsemote/" + String(t->gettabname());
  topic.replace("-","");
  topic.toLowerCase();
  char outputChar[topic.length() + 1];
  topic.toCharArray(outputChar, topic.length() + 1);
  if (status == SYNC_ON) 
    mqttsend(outputChar,"on");
  if (status == SYNC_OFF) 
    mqttsend(outputChar,"off");
  if (status == SYNC_START) 
    mqttsend(outputChar,"hello");
  if (status == SYNC_BYE) 
    mqttsend(outputChar,"bye");  
}

// Add a device menu

// Callback function for Close (X) button
void tab_mqtt::popup_add_device_close_event_cb(lv_event_t * e) {
    tab_mqtt *t = static_cast<tab_mqtt *>(lv_event_get_user_data(e));
    lv_obj_del(t->popup_add_device_modal); // Close the message box
    t->popup_add_device_open = false;
}

// Callback function for list selection
void tab_mqtt::popup_add_device_list_event_handler(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
  tab_mqtt *t = static_cast<tab_mqtt *>(lv_event_get_user_data(e));
    
  if(code == LV_EVENT_CLICKED) {
    if(t->selected_btn == btn) {
      // Unhighlight the button if it's already selected
      lv_obj_clear_state(t->selected_btn, LV_STATE_CHECKED);
                  lv_obj_remove_style(t->selected_btn, &t->style_selected, 0);

       t->selected_btn = NULL;
    } else {
      // Unhighlight the previous button if there was one
      if(t->selected_btn != NULL) {
        lv_obj_clear_state(t->selected_btn, LV_STATE_CHECKED);
                        lv_obj_remove_style(t->selected_btn, &t->style_selected, 0);

      }
      // Highlight the current button
      lv_obj_add_state(btn, LV_STATE_CHECKED);
                  lv_obj_add_style(btn, &t->style_selected, 0);

      t->selected_btn = btn;
    }
  }
}

void tab_mqtt::popup_add_device_ok_event_cb(lv_event_t * e) {
  tab_mqtt *t = static_cast<tab_mqtt *>(lv_event_get_user_data(e));
  if(t->selected_btn != NULL) {
      //const char * txt = lv_list_get_btn_text(selected_btn);
      //printf("Selected option: %s\n", txt);
      // Perform the desired action with the selected option here
  }
  lv_obj_del(t->popup_add_device_modal); // Close the message box
  t->popup_add_device_open = false;
}

void tab_mqtt::popup_add_device(lv_obj_t *base) {
    if (popup_add_device_open) {
        lv_obj_del(popup_add_device_modal); // Close the message box
        popup_add_device_open = false;
        return;
    }
    popup_add_device_open = true;
    selected_btn = NULL;

    lv_style_init(&style_selected);
    lv_style_set_bg_color(&style_selected, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_selected, LV_OPA_50);

    popup_add_device_modal = lv_obj_create(base);
    lv_obj_set_size(popup_add_device_modal, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(popup_add_device_modal, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(popup_add_device_modal, lv_color_black(), 0);
    lv_obj_align(popup_add_device_modal, LV_ALIGN_CENTER, 0, 0);

    // Create a container for the message box content
    lv_obj_t * container = lv_obj_create(popup_add_device_modal);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_size(container, 320, 150); // todo better dynamic
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);

    // Create a label for the message box title
    lv_obj_t * title = lv_label_create(container);
    lv_label_set_text(title, "Add a device");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 10);

    // Create a list
    lv_obj_t * list = lv_list_create(container);
    lv_obj_set_size(list, 320, 100); // todo better dynamic
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);

    // Add items to the list
    for(int i = 1; i <= 3; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Option %d", i);
        lv_obj_t * list_btn = lv_list_add_btn(list, NULL, buf);
        lv_obj_add_event_cb(list_btn, popup_add_device_list_event_handler, LV_EVENT_CLICKED, this);
    }

    // Create a container for the buttons
    lv_obj_t * btn_container = lv_obj_create(container);
    lv_obj_set_width(btn_container, lv_pct(100));
    lv_obj_set_height(btn_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(btn_container, 10, 0);

    // Create OK button
    lv_obj_t * ok_btn = lv_btn_create(btn_container);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_add_event_cb(ok_btn, popup_add_device_ok_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t * ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "OK");
    lv_obj_center(ok_label);

    // Create Close (X) button
    lv_obj_t * close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 100, 40);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
    lv_obj_add_event_cb(close_btn, popup_add_device_close_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t * close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "CANCEL");
    lv_obj_center(close_label);

}