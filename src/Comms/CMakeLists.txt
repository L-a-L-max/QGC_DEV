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

    # --- IDL Type Generation via idlc ---
    find_program(IDLC_EXECUTABLE idlc REQUIRED)
    set(DDS_IDL_DIR "${CMAKE_SOURCE_DIR}/src/DDS/idl")
    set(DDS_IDL_GEN_DIR "${CMAKE_BINARY_DIR}/dds_generated")
    file(MAKE_DIRECTORY ${DDS_IDL_GEN_DIR})

    file(GLOB DDS_IDL_FILES "${DDS_IDL_DIR}/*.idl")

    set(DDS_GENERATED_SOURCES "")
    set(DDS_GENERATED_HEADERS "")
    foreach(idl_file ${DDS_IDL_FILES})
        get_filename_component(idl_name ${idl_file} NAME_WE)
        set(gen_c "${DDS_IDL_GEN_DIR}/${idl_name}.c")
        set(gen_h "${DDS_IDL_GEN_DIR}/${idl_name}.h")
        add_custom_command(
            OUTPUT ${gen_c} ${gen_h}
            COMMAND ${IDLC_EXECUTABLE} -l c -t -o ${DDS_IDL_GEN_DIR} ${idl_file}
            DEPENDS ${idl_file}
            COMMENT "idlc: Generating ${idl_name}.h/.c from ${idl_name}.idl"
        )
        list(APPEND DDS_GENERATED_SOURCES ${gen_c})
        list(APPEND DDS_GENERATED_HEADERS ${gen_h})
    endforeach()

    # --- IDL Type Generation — versioned overrides ---
    # Helper macro: compile IDL files from a subdirectory with a name suffix.
    # The suffix is appended to each generated .c/.h filename so that symbols
    # from different versions do not collide.
    macro(dds_compile_idl_variant IDL_VARIANT_DIR IDL_SUFFIX)
        set(_VAR_GEN_DIR "${CMAKE_BINARY_DIR}/dds_generated${IDL_SUFFIX}")
        file(MAKE_DIRECTORY ${_VAR_GEN_DIR})
        file(GLOB _VAR_IDL_FILES "${IDL_VARIANT_DIR}/*.idl")
        foreach(_idl_file ${_VAR_IDL_FILES})
            get_filename_component(_idl_name ${_idl_file} NAME_WE)
            set(_idl_copy "${_VAR_GEN_DIR}/${_idl_name}${IDL_SUFFIX}.idl")
            set(_gen_c "${_VAR_GEN_DIR}/${_idl_name}${IDL_SUFFIX}.c")
            set(_gen_h "${_VAR_GEN_DIR}/${_idl_name}${IDL_SUFFIX}.h")
            add_custom_command(
                OUTPUT ${_gen_c} ${_gen_h}
                COMMAND ${CMAKE_COMMAND} -E copy ${_idl_file} ${_idl_copy}
                COMMAND ${IDLC_EXECUTABLE} -l c -t -o ${_VAR_GEN_DIR} ${_idl_copy}
                DEPENDS ${_idl_file}
                COMMENT "idlc: Generating ${_idl_name}${IDL_SUFFIX}.h/.c"
            )
            list(APPEND DDS_GENERATED_SOURCES ${_gen_c})
            list(APPEND DDS_GENERATED_HEADERS ${_gen_h})
        endforeach()
        list(APPEND DDS_VARIANT_GEN_DIRS ${_VAR_GEN_DIR})
    endmacro()

    set(DDS_VARIANT_GEN_DIRS "")

    # V4 (latest px4_msgs VehicleStatus layout)
    dds_compile_idl_variant("${CMAKE_SOURCE_DIR}/src/DDS/idl/v4" "_v4")

    # PX4 v1.16 overrides (CUAV X7+ firmware)
    dds_compile_idl_variant("${CMAKE_SOURCE_DIR}/src/DDS/idl/px4_v116" "_v116")

    # Build generated C files as a static library (avoids C/C++ PCH conflicts)
    add_library(dds_idl_types STATIC ${DDS_GENERATED_SOURCES} ${DDS_GENERATED_HEADERS})
    target_include_directories(dds_idl_types
        PUBLIC ${DDS_IDL_GEN_DIR} ${DDS_VARIANT_GEN_DIRS}
        PRIVATE ${CycloneDDS_INCLUDE_DIR}
    )
    target_link_libraries(dds_idl_types PRIVATE CycloneDDS::ddsc)
    set_target_properties(dds_idl_types PROPERTIES
        C_STANDARD 11
        POSITION_INDEPENDENT_CODE ON
    )

    # --- DDS Source Files ---
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
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSTypeRegistry.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSTypeRegistry.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSVehicleManager.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSVehicleManager.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSCommandPublisher.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSCommandPublisher.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSHeartbeatPublisher.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSHeartbeatPublisher.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSManualControlPublisher.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSManualControlPublisher.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSGotoPublisher.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSGotoPublisher.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSMissionManager.cc
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSMissionManager.h
            ${CMAKE_SOURCE_DIR}/src/DDS/DDSWaypoint.h
    )

    target_include_directories(${CMAKE_PROJECT_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/DDSLink
            ${CMAKE_SOURCE_DIR}/src/DDS
            ${DDS_IDL_GEN_DIR}
            ${DDS_VARIANT_GEN_DIRS}
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QGC_ENABLE_DDS)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CycloneDDS::ddsc dds_idl_types)

    if(ANDROID)
        set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY
            QT_ANDROID_EXTRA_LIBS "${CycloneDDS_LIBRARY}"
        )
    endif()

    qt_add_resources(${CMAKE_PROJECT_NAME} dds_mappings
        PREFIX "/dds_mappings"
        BASE "${CMAKE_SOURCE_DIR}/resources/dds_mappings"
        FILES
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/_default.json"
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/_vendor_template.json"
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/_default_v4.json"
            "${CMAKE_SOURCE_DIR}/resources/dds_mappings/cuav_x7pro.json"
    )
endif()

# ============================================================================
# Communication Link Subdirectories
# ============================================================================

add_subdirectory(MockLink)
