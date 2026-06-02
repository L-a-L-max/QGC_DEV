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
    )

    target_include_directories(${CMAKE_PROJECT_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/DDSLink
            ${CMAKE_SOURCE_DIR}/src/DDS
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QGC_ENABLE_DDS)

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
