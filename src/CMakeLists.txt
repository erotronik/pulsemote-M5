# ESP-IDF component for src/ directory (temporary until migration to main/)
# This allows building with ESP-IDF while keeping PlatformIO compatibility

idf_component_register(
    SRCS "main.cpp"
         "comms-bt.cpp"
         "comms-wifi.cpp"
         "board-m5cores3.cpp"
         "board-m5tab5.cpp"
         "board-waveshare7.cpp"
         "device.cpp"
         "device-bubblebottle.cpp"
         "device-dgbutton.cpp"
         "device-funosr.cpp"
         "device-loop.cpp"
         "device-lovense.cpp"
         "device-mk312.cpp"
         "device-ossm.cpp"
         "device-thrustalot.cpp"
         "lvgl-utils.cpp"
         "tab-bubblebottle.cpp"
         "tab-coyote.cpp"
         "tab-dgbutton.cpp"
         "tab-funosr.cpp"
         "tab-loop.cpp"
         "tab-lovense.cpp"
         "tab-mk312.cpp"
         "tab-mqtt.cpp"
         "tab-mqtt-leds.cpp"
         "tab-mqtt-socket.cpp"
         "tab-object-buttonbar.cpp"
         "tab-object-modes.cpp"
         "tab-object-patterns.cpp"
         "tab-object-sync.cpp"
         "tab-object-timer.cpp"
         "tab-ossm.cpp"
         "tab-splashscreen.cpp"
         "tab-thrustalot.cpp"
    INCLUDE_DIRS "../include"
    REQUIRES esp-nimble-cpp lvgl m5-pulsemote-pcb CoyoStim Venerate Rotary-master driver esp_timer ArduinoJson
    PRIV_REQUIRES espressif__esp_hosted
    )

if(CONFIG_BOARD_M5TAB5)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_M5TAB5 BOARD_HAS_PSRAM)
elseif(CONFIG_BOARD_M5CORES3)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_M5CORES3 PULSEMOTE_PCB)
endif()
