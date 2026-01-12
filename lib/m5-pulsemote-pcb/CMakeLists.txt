idf_component_register(
    SRCS "src/pulsemote-pcb.cpp"
         "src/pca9685.cpp"
         "src/mcp23017.cpp"
         "src/rotaryencoders.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer lvgl
)
