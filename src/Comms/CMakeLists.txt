# ============================================================================
# Communications Module
# Handles all communication links (TCP, UDP, Serial, Bluetooth, etc.)
# ============================================================================

# ----------------------------------------------------------------------------
# Core Communication Sources
# ----------------------------------------------------------------------------
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        LinkConfiguration.cc
        LinkConfiguration.h
        LinkInterface.cc
        LinkInterface.h
        LinkManager.cc
        LinkManager.h
        LogReplayLink.cc
        LogReplayLink.h
        LogReplayLinkController.cc
        LogReplayLinkController.h
        MAVLinkProtocol.cc
        MAVLinkProtocol.h
        TCPLink.cc
        TCPLink.h
        UdpIODevice.cc
        UdpIODevice.h
        UDPLink.cc
        UDPLink.h
)

# USB board information JSON data
qt_add_resources(${CMAKE_PROJECT_NAME} json_comm
    PREFIX "/json"
    FILES USBBoardInfo.json
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Uncomment to enable Qt socket debugging
# target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QABSTRACTSOCKET_DEBUG)

# ============================================================================
# Optional Communication Features
# ============================================================================

# ----------------------------------------------------------------------------
# Serial Port Communication
# ----------------------------------------------------------------------------

if(QGC_NO_SERIAL_LINK)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QGC_NO_SERIAL_LINK)
else()
    target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
            QGCSerialPortInfo.cc
            QGCSerialPortInfo.h
            SerialLink.cc
            SerialLink.h
    )

    if(NOT ANDROID)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::SerialPort)
    endif()
endif()

# ----------------------------------------------------------------------------
# Bluetooth Communication
# ----------------------------------------------------------------------------
add_subdirectory(Bluetooth)

# ----------------------------------------------------------------------------
# DDS Communication (optional, requires CycloneDDS)
# ----------------------------------------------------------------------------
if(QGC_ENABLE_DDS)
    # Find CycloneDDS — supports CMake config, pkg-config, and manual search.
    # If CycloneDDS is in a custom location, pass -DCYCLONEDDS_ROOT=/path/to/cyclonedds
    # or set CYCLONEDDS_HOME environment variable.
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    find_package(CycloneDDS REQUIRED)

    target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
            DDSLink/DDSConfiguration.cc
            DDSLink/DDSConfiguration.h
            DDSLink/DDSLink.cc
            DDSLink/DDSLink.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSMappingEngine.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSMappingEngine.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSTransformRegistry.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSTransformRegistry.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSDataInjector.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSDataInjector.h
            # CycloneDDS-generated PX4 message types
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleAttitude.c
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleAttitude.h
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleGlobalPosition.c
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleGlobalPosition.h
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleLocalPosition.c
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleLocalPosition.h
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/BatteryStatus.c
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/BatteryStatus.h
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleStatus.c
            ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleStatus.h
    )

    target_include_directories(${CMAKE_PROJECT_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/DDSLink
            ${CMAKE_SOURCE_DIR}/src/DDS
            ${CMAKE_SOURCE_DIR}/src/DDS/generated
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QGC_ENABLE_DDS)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CycloneDDS::ddsc)

    # Suppress warnings in generated C code
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleAttitude.c
        ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleGlobalPosition.c
        ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleLocalPosition.c
        ${CMAKE_SOURCE_DIR}/src/DDS/generated/BatteryStatus.c
        ${CMAKE_SOURCE_DIR}/src/DDS/generated/VehicleStatus.c
        PROPERTIES COMPILE_FLAGS "-w"
    )

    qt_add_resources(${CMAKE_PROJECT_NAME} dds_mappings
        PREFIX "/dds_mappings"
        BASE "${CMAKE_SOURCE_DIR}/resources/dds_mappings"
        FILES
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/_default.json"
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/_vendor_template.json"
    )
endif()

# ============================================================================
# Communication Link Subdirectories
# ============================================================================

add_subdirectory(MockLink)
