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
# Creates imported target:
#   CycloneDDS::ddsc       - The CycloneDDS C library
#
# Supports:
#   1. CycloneDDS CMake config (CycloneDDSConfig.cmake)
#   2. pkg-config (cyclonedds)
#   3. Manual search in common install paths
#   4. CYCLONEDDS_HOME / CYCLONEDDS_ROOT environment hints

# ── Strategy 1: Try the CMake config (CycloneDDS ships CycloneDDSConfig.cmake)
find_package(CycloneDDS CONFIG QUIET)
if(CycloneDDS_FOUND AND TARGET CycloneDDS::ddsc)
    message(STATUS "Found CycloneDDS via CMake config: ${CycloneDDS_VERSION}")
    return()
endif()

if(CycloneDDS_FOUND AND NOT TARGET CycloneDDS::ddsc)
    message(STATUS "CycloneDDS CMake config found (${CycloneDDS_VERSION}) "
                   "but CycloneDDS::ddsc target missing — falling back to manual search")
    set(CycloneDDS_FOUND FALSE)
endif()

# ── Strategy 2: Try pkg-config
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(_CDDS QUIET cyclonedds)
    if(_CDDS_FOUND)
        set(CycloneDDS_INCLUDE_DIR "${_CDDS_INCLUDE_DIRS}" CACHE PATH "CycloneDDS include dir")
        find_library(CycloneDDS_LIBRARY
            NAMES ddsc
            HINTS ${_CDDS_LIBRARY_DIRS}
        )
        if(NOT CycloneDDS_VERSION)
            set(CycloneDDS_VERSION "${_CDDS_VERSION}")
        endif()
    endif()
endif()

# ── Strategy 3: Manual search with broad path list
if(NOT CycloneDDS_INCLUDE_DIR)
    find_path(CycloneDDS_INCLUDE_DIR
        NAMES dds/dds.h
        PATHS
            /usr/local/include
            /usr/include
            /opt/cyclonedds/include
            /opt/ros/humble/include
            /opt/ros/iron/include
            /opt/ros/jazzy/include
        PATH_SUFFIXES cyclonedds
        HINTS
            $ENV{CYCLONEDDS_HOME}/include
            ${CYCLONEDDS_ROOT}/include
    )
endif()

if(NOT CycloneDDS_LIBRARY)
    find_library(CycloneDDS_LIBRARY
        NAMES ddsc
        PATHS
            /usr/local/lib
            /usr/local/lib/x86_64-linux-gnu
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /opt/cyclonedds/lib
            /opt/ros/humble/lib
            /opt/ros/iron/lib
            /opt/ros/jazzy/lib
        HINTS
            $ENV{CYCLONEDDS_HOME}/lib
            ${CYCLONEDDS_ROOT}/lib
    )
endif()

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

    message(STATUS "CycloneDDS: ${CycloneDDS_LIBRARY}")
    message(STATUS "CycloneDDS include: ${CycloneDDS_INCLUDE_DIR}")
endif()

mark_as_advanced(CycloneDDS_INCLUDE_DIR CycloneDDS_LIBRARY)
