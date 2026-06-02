cmake_minimum_required(VERSION 3.25)

# Standalone test build for DDS mapping engine
# Usage:
#   mkdir build && cd build
#   cmake .. -DQGC_ENABLE_DDS=ON
#   make
#   ./test_dds_mapping

project(test_dds_mapping LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

option(QGC_ENABLE_DDS "Enable DDS support" ON)

if(QGC_ENABLE_DDS)
    add_compile_definitions(QGC_ENABLE_DDS)
endif()

set(DDS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(test_dds_mapping
    test_mapping_engine.cc
    ${DDS_SRC_DIR}/DDS/DDSMappingEngine.cc
    ${DDS_SRC_DIR}/DDS/DDSTransformRegistry.cc
    ${DDS_SRC_DIR}/DDS/DDSDataInjector.cc
)

target_include_directories(test_dds_mapping PRIVATE
    ${DDS_SRC_DIR}/DDS
    ${DDS_SRC_DIR}/Comms/DDSLink
)

target_link_libraries(test_dds_mapping PRIVATE
    Qt6::Core
)

# CTest integration
enable_testing()
add_test(NAME DDSMappingEngineTest COMMAND test_dds_mapping)
