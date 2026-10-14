# Custom OLED status screen for the dongle only (skipped when a build picks the built-in screen)
if(CONFIG_SHIELD_LILY58_DONGLE AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boards/shields/lily58_dongle/custom_status_screen.c)
endif()
