# Custom OLED status screen for the dongle only
if(CONFIG_SHIELD_LILY58_DONGLE)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/boards/shields/lily58_dongle/custom_status_screen.c)
endif()
