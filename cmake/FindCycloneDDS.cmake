# FindCycloneDDS.cmake
#
# Find the Eclipse Cyclone DDS library.
#
# Sets:
#   CycloneDDS_FOUND       - TRUE if CycloneDDS is found
#   CycloneDDS_INCLUDE_DIRS - Include directories
#   CycloneDDS_LIBRARIES   - Libraries to link against
#   CycloneDDS_VERSION     - Version string
#
# Usage:
#   find_package(CycloneDDS REQUIRED)
#   target_link_libraries(myapp CycloneDDS::ddsc)

# Try the CMake config first (CycloneDDS ships with CycloneDDSConfig.cmake)
find_package(CycloneDDS CONFIG QUIET)
if(CycloneDDS_FOUND)
    message(STATUS "Found CycloneDDS via CMake config: ${CycloneDDS_VERSION}")
    return()
endif()

# Fallback: manual search
find_path(CycloneDDS_INCLUDE_DIR
    NAMES dds/dds.h
    PATHS
        /usr/local/include
        /usr/include
        $ENV{CYCLONEDDS_HOME}/include
        ${CYCLONEDDS_ROOT}/include
)

find_library(CycloneDDS_LIBRARY
    NAMES ddsc
    PATHS
        /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        $ENV{CYCLONEDDS_HOME}/lib
        ${CYCLONEDDS_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CycloneDDS
    REQUIRED_VARS CycloneDDS_LIBRARY CycloneDDS_INCLUDE_DIR
)

if(CycloneDDS_FOUND)
    set(CycloneDDS_LIBRARIES ${CycloneDDS_LIBRARY})
    set(CycloneDDS_INCLUDE_DIRS ${CycloneDDS_INCLUDE_DIR})

    if(NOT TARGET CycloneDDS::ddsc)
        add_library(CycloneDDS::ddsc UNKNOWN IMPORTED)
        set_target_properties(CycloneDDS::ddsc PROPERTIES
            IMPORTED_LOCATION "${CycloneDDS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CycloneDDS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(CycloneDDS_INCLUDE_DIR CycloneDDS_LIBRARY)
