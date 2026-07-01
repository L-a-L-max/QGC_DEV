# ============================================================================
# G16 Joystick Module
# Skydroid G16 remote controller integration for Android
# Maps G16 channel data to QGC's virtual joystick (MAVLink MANUAL_CONTROL)
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        G16JoystickPlugin.cc
        G16JoystickPlugin.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
