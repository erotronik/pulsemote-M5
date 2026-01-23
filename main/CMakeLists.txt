# ESP-IDF component for src/ directory (temporary until migration to main/)
# This allows building with ESP-IDF while keeping PlatformIO compatibility

idf_component_register(
    SRCS "core/main.cpp"
         "core/device.cpp"
         "core/lvgl-utils.cpp"
         "comms/comms-bt.cpp"
         "comms/comms-wifi.cpp"
         "boards/board-m5cores3.cpp"
         "boards/board-m5tab5.cpp"
         "boards/board-waveshare7.cpp"
         "devices/device-bubblebottle.cpp"
         "devices/device-dgbutton.cpp"
         "devices/device-funosr.cpp"
         "devices/device-loop.cpp"
         "devices/device-lovense.cpp"
         "devices/device-mk312.cpp"
         "devices/device-ossm.cpp"
         "devices/device-thrustalot.cpp"
	 "ui/tab.cpp"
         "ui/tabs/tab-bubblebottle.cpp"
         "ui/tabs/tab-coyote.cpp"
         "ui/tabs/tab-dgbutton.cpp"
         "ui/tabs/tab-funosr.cpp"
         "ui/tabs/tab-loop.cpp"
         "ui/tabs/tab-lovense.cpp"
         "ui/tabs/tab-mk312.cpp"
         "ui/tabs/tab-mqtt.cpp"
         "ui/tabs/tab-mqtt-leds.cpp"
         "ui/tabs/tab-mqtt-socket.cpp"
         "ui/widgets/tab-object-buttonbar.cpp"
         "ui/widgets/tab-object-modes.cpp"
         "ui/widgets/tab-object-patterns.cpp"
         "ui/widgets/tab-object-sync.cpp"
         "ui/widgets/tab-object-timer.cpp"
         "ui/widgets/tab-object-status.cpp"
         "ui/widgets/tab-object-segbar.cpp"
         "ui/widgets/tab-object-modetimer.cpp"
         "ui/tabs/tab-ossm.cpp"
         "ui/tabs/tab-splashscreen.cpp"
         "ui/tabs/tab-thrustalot.cpp"
    INCLUDE_DIRS "core" "comms" "boards" "devices" "ui" "ui/tabs" "ui/widgets"
    REQUIRES esp-nimble-cpp lvgl m5-pulsemote-pcb CoyoStim Venerate Rotary-master driver esp_timer ArduinoJson
    )

if(CONFIG_BOARD_M5TAB5)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_HAS_PSRAM)
endif()
