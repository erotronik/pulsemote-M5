idf_component_register(
    SRCS "src/pulsemote-pcb.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lvgl driver esp_timer M5Unified
)
