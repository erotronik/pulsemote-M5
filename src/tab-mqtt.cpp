#include "tab.hpp"
#include "device.hpp"
#include "lvgl-utils.h"
#include "comms-wifi.hpp"
#include "config.h"
#include "tab-mqtt.hpp"

#ifdef CONFIG_MQTT_SERVER
#include "tab-mqtt-socket.hpp"
#include "tab-mqtt-stroker.hpp"
#include "tab-mqtt-leds.hpp"
#endif

#include <freertos/queue.h>
#include <freertos/task.h>

tab_mqtt::tab_mqtt() {
  //events = xQueueCreate(10,sizeof(sync_data));
}

void tab_mqtt::loop(boolean activetab) {
  //sync_data syncstatus;
  //if (xQueueReceive(events, &syncstatus, 0))
  //  send_sync_data(syncstatus);

   mqttsenditem item;

  if (xQueueReceive(mqttsubhandle, &item, 0)) {
    ESP_LOGD("","%s=%s",item.topic,item.message);
    if (!strncmp(item.topic,"wled/",4)) {
      char *first = strchr(item.topic,'/');                 // after "wled"
      if (first) {
        char *second = strchr(first+1,'/');
        if (second) {
          char mid[35];
          size_t len = second - (first + 1);
          if (len >= sizeof(mid)) len = sizeof(mid) - 1;
          strncpy(mid, first + 1, len);
          mid[len] = '\0';

          if (!strcmp(item.message,"online")) {
            char topic[100];
            snprintf(topic,sizeof(topic)-1, "wled/%s", mid);
            ESP_LOGD("popup","creating %s",topic);
            boolean exists = false;
            for (const auto& allt : tabs) {
              if (!strcasecmp(allt->gettabname(),mid))
                exists = true;
            }
            if (!exists) {
              tab_mqtt_leds *mv = new tab_mqtt_leds(mid, topic);
              mv->setup();
              mv->focus_change(true);
              tabs.emplace_back(mv);
            }
          }
          if (!strcmp(item.message,"offline")) {
            for (const auto& allt : tabs) {
              if (!strcasecmp(allt->gettabname(),mid)) {
                ESP_LOGE("mqtt","found tab %s delete", allt->gettabname());
                allt->last_change = D_DISCONNECTED;
              }
            }
          }
        }
      }
    }
  }

  wifi_loop();
}

const char *tab_mqtt::geticons(void) {
  return (is_wifi_connected()?LV_SYMBOL_WIFI:"");
}

const char* tab_mqtt::gettabname(void){ 
  return "wifi";
}

void tab_mqtt::setup(void) {
  tab_mqtt *mt = static_cast<tab_mqtt *>(this);
  mt->type = DeviceType::device_mqtt;
  mt->device = nullptr;
  mt->last_change = mt->old_last_change = D_NONE;
  wifi_setup();  
  mqttsubscribe("wled/+/status");
}

#if 0
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
#endif

void tab_mqtt::gotsyncdata(Tab *t, sync_data status) {
  char topic[max_topic_size];
  ESP_LOGD("mqtt", "got sync data %d from %s", status, t->gettabname());
  snprintf(topic, max_topic_size-1, "pulsemote/%s", t->gettabname());

  // Replace '-' with ''
  int j = 0;
  for (int i = 0; topic[i] != '\0'; i++) {
    if (topic[i] != '-')
      topic[j++] = tolower(topic[i]);
  }
  topic[j] = '\0';
  if (status == SYNC_ON) 
    mqttsend(topic,"on");
  if (status == SYNC_OFF) 
    mqttsend(topic,"off");
  if (status == SYNC_START) 
    mqttsend(topic,"hello");
  if (status == SYNC_BYE) 
    mqttsend(topic,"bye");  
  ESP_LOGD("mqtt","sent message");
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
    char * txt = lv_label_get_text(lv_obj_get_child(t->selected_btn,0));
    ESP_LOGD("popup","selected %s",txt);

#ifdef CONFIG_MQTT_SERVER
    if (!strncmp(txt,"socket",6)) {
      char topic[100];
      snprintf(topic,sizeof(topic)-1, "zigbee2mqtt/%s/set", txt);
      ESP_LOGD("popup","creating %s",topic);
      boolean exists = false;
      for (const auto& allt : tabs) {
        if (!strcasecmp(allt->gettabname(),txt))
          exists = true;
      }
      if (!exists) {
        tab_mqtt_socket *mv = new tab_mqtt_socket(txt, topic);
        mv->setup();
        mv->focus_change(true);
        tabs.emplace_back(mv);
      }
    }
    if (!strncmp(txt,"leds ",4)) {
      char topic[100];
      snprintf(topic,sizeof(topic)-1, "wled/%s", txt+5);
      ESP_LOGD("popup","creating %s",topic);
      boolean exists = false;
      for (const auto& allt : tabs) {
        if (!strcasecmp(allt->gettabname(),txt+5))
          exists = true;
      }
      if (!exists) {
        tab_mqtt_leds *mv = new tab_mqtt_leds(txt+5, topic);
        mv->setup();
        mv->focus_change(true);
        tabs.emplace_back(mv);
      }
    }
    if (!strncasecmp(txt,"stroker",7)) {
      boolean exists = false;
      for (const auto& allt : tabs) {
        if (!strcasecmp(allt->gettabname(),txt))
          exists = true;
      }
      if (!exists) {
        tab_stroker *mv = new tab_stroker();
        mv->setup();
        mv->focus_change(true);
        tabs.emplace_back(mv);
      }
    }
#endif
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
    lv_obj_set_style_pad_all(popup_add_device_modal,0, LV_PART_MAIN);
    lv_obj_set_size(popup_add_device_modal, LV_PCT(100), LV_PCT(100));
    lv_obj_align(popup_add_device_modal, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_flex_flow(popup_add_device_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(popup_add_device_modal, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_t * title = lv_label_create(popup_add_device_modal);
    lv_label_set_text(title, "Add a device");

    // Create a list
    lv_obj_t * list = lv_list_create(popup_add_device_modal);
    lv_obj_set_height(list, 100); // todo better dynamic
    lv_obj_set_width(list, LV_PCT(100));

#ifdef CONFIG_WIFI_SSID
    for (const std::string &name : mqtt_device_list) {
      lv_obj_t *list_btn = lv_list_add_button(list, NULL, name.c_str());
      lv_obj_add_event_cb(list_btn, popup_add_device_list_event_handler, LV_EVENT_CLICKED, this);
    }
#endif

    // Create a container for the buttons
    lv_obj_t * btn_container = lv_obj_create(popup_add_device_modal);
    lv_obj_set_width(btn_container, lv_pct(100));
    lv_obj_set_height(btn_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(btn_container, 5, 0);

    // Create OK button
    lv_obj_t * ok_btn = lv_btn_create(btn_container);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_add_event_cb(ok_btn, popup_add_device_ok_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t * ok_label = lv_label_create(ok_btn);
    lv_label_set_text(ok_label, "Ok");
    lv_obj_center(ok_label);

    // Create Close (X) button
    lv_obj_t * close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 100, 40);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
    lv_obj_add_event_cb(close_btn, popup_add_device_close_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t * close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Cancel");
    lv_obj_center(close_label);

}