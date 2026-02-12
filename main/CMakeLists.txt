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
         "devices/bubblebottle/device-bubblebottle.cpp"
         "devices/bubblebottle/tab-bubblebottle.cpp"
         "devices/coyote/tab-coyote.cpp"
         "devices/dgbutton/device-dgbutton.cpp"
         "devices/dgbutton/tab-dgbutton.cpp"
         "devices/funosr/device-funosr.cpp"
         "devices/funosr/tab-funosr.cpp"
         "devices/loop/device-loop.cpp"
         "devices/loop/tab-loop.cpp"
         "devices/lovense/device-lovense.cpp"
         "devices/lovense/tab-lovense.cpp"
         "devices/mk312/device-mk312.cpp"
         "devices/mk312/tab-mk312.cpp"
         "devices/mqtt-leds/tab-mqtt-leds.cpp"
         "devices/mqtt-socket/tab-mqtt-socket.cpp"
         "devices/ossm/device-ossm.cpp"
         "devices/ossm/tab-ossm.cpp"
         "devices/thrustalot/device-thrustalot.cpp"
         "devices/thrustalot/tab-thrustalot.cpp"
	     "ui/tab.cpp"
         "ui/tab-mqtt.cpp"
         "ui/tab-splashscreen.cpp"
         "ui/widgets/tab-object-buttonbar.cpp"
         "ui/widgets/tab-object-modes.cpp"
         "ui/widgets/tab-object-modetimer.cpp"
         "ui/widgets/tab-object-patterns.cpp"
         "ui/widgets/tab-object-segbar.cpp"
         "ui/widgets/tab-object-status.cpp"
         "ui/widgets/tab-object-sync.cpp"
         "ui/widgets/tab-object-timer.cpp"
    INCLUDE_DIRS "core" "comms" "boards" "devices" "ui" "ui/widgets"
                 "devices/bubblebottle" "devices/coyote" "devices/dgbutton" "devices/funosr" 
                 "devices/loop" "devices/lovense" "devices/mk312" "devices/ossm" "devices/thrustalot"
    REQUIRES esp-nimble-cpp lvgl m5-pulsemote-pcb CoyoStim Venerate Rotary-master driver esp_timer ArduinoJson  m5-encoder8
    )

if(CONFIG_BOARD_M5TAB5)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_HAS_PSRAM)
endif()
