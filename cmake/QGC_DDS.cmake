# QGC_DDS.cmake
#
# CMake integration for QGC DDS support.
# Include this from the top-level CMakeLists.txt:
#
#   option(QGC_ENABLE_DDS "Enable DDS communication support" OFF)
#   if(QGC_ENABLE_DDS)
#       include(cmake/QGC_DDS.cmake)
#   endif()
#
# This module:
#   1. Finds CycloneDDS
#   2. Collects DDS source files
#   3. Defines the compile definition QGC_ENABLE_DDS
#   4. Adds DDS mapping resources to the Qt resource system

# --- Find DDS library ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
find_package(CycloneDDS REQUIRED)

# --- DDS source files ---
set(QGC_DDS_SOURCES
    src/Comms/DDSLink/DDSLink.h
    src/Comms/DDSLink/DDSLink.cc
    src/Comms/DDSLink/DDSConfiguration.h
    src/Comms/DDSLink/DDSConfiguration.cc
    src/DDS/DDSMappingEngine.h
    src/DDS/DDSMappingEngine.cc
    src/DDS/DDSTransformRegistry.h
    src/DDS/DDSTransformRegistry.cc
    src/DDS/DDSDataInjector.h
    src/DDS/DDSDataInjector.cc
)

# --- Add sources to the main target ---
# Usage in top-level CMakeLists.txt:
#   target_sources(${PROJECT_NAME} PRIVATE ${QGC_DDS_SOURCES})
#   target_link_libraries(${PROJECT_NAME} PRIVATE CycloneDDS::ddsc)
#   target_compile_definitions(${PROJECT_NAME} PRIVATE QGC_ENABLE_DDS)

# --- DDS mapping resources ---
# These JSON files are embedded as Qt resources so they ship with the binary.
set(QGC_DDS_RESOURCES
    resources/dds_mappings/_default.json
    resources/dds_mappings/_vendor_template.json
)

# Generate a .qrc file for the DDS mappings
set(QGC_DDS_QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/dds_mappings.qrc")
file(WRITE ${QGC_DDS_QRC_FILE} "<RCC>\n  <qresource prefix=\"/dds_mappings\">\n")
foreach(mapping_file ${QGC_DDS_RESOURCES})
    get_filename_component(fname ${mapping_file} NAME)
    file(WRITE ${QGC_DDS_QRC_FILE} "    <file alias=\"${fname}\">${CMAKE_SOURCE_DIR}/${mapping_file}</file>\n" APPEND)
endforeach()
file(WRITE ${QGC_DDS_QRC_FILE} "  </qresource>\n</RCC>\n" APPEND)

message(STATUS "QGC DDS support enabled")
message(STATUS "  CycloneDDS: ${CycloneDDS_LIBRARIES}")
message(STATUS "  DDS sources: ${QGC_DDS_SOURCES}")
message(STATUS "  DDS mappings: ${QGC_DDS_RESOURCES}")
