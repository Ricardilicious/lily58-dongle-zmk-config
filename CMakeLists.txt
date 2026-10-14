# Clean OLED status screen for the dongle
if(CONFIG_SHIELD_LILY58_DONGLE AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM AND NOT CONFIG_SHIELD_DONGLE_DISPLAY)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boards/shields/lily58_dongle/custom_status_screen.c)
endif()
