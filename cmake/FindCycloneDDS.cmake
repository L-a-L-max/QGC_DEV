# FindCycloneDDS.cmake
#
# Find the Eclipse Cyclone DDS library (libddsc).
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
#   1. pkg-config (cyclonedds)
#   2. Manual search in common install paths
#   3. CYCLONEDDS_HOME env / CYCLONEDDS_ROOT cmake variable hints
#
# NOTE: We intentionally do NOT use find_package(CycloneDDS CONFIG) because
# some CycloneDDS installations provide a CycloneDDSConfig.cmake that sets
# CycloneDDS_FOUND but creates IMPORTED targets whose scope does not survive
# CMake's generate phase when invoked from a Find module (observed with
# CMake 4.x). By always locating the library and headers ourselves and
# creating a GLOBAL IMPORTED target, we guarantee the target is available
# everywhere in the project.

# ── Strategy 1: Try pkg-config for path hints
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

# ── Strategy 2: Try CycloneDDS's own cmake config just for path extraction
#    (don't rely on its targets — we create our own below)
if(NOT CycloneDDS_INCLUDE_DIR OR NOT CycloneDDS_LIBRARY)
    find_package(CycloneDDS CONFIG QUIET)
    if(CycloneDDS_FOUND)
        # Extract paths from the config-created target if it exists
        if(TARGET CycloneDDS::ddsc)
            get_target_property(_cdds_loc CycloneDDS::ddsc IMPORTED_LOCATION)
            get_target_property(_cdds_inc CycloneDDS::ddsc INTERFACE_INCLUDE_DIRECTORIES)
            if(NOT _cdds_loc)
                # Try per-configuration location
                get_target_property(_cdds_loc CycloneDDS::ddsc IMPORTED_LOCATION_RELEASE)
            endif()
            if(NOT _cdds_loc)
                get_target_property(_cdds_loc CycloneDDS::ddsc IMPORTED_LOCATION_NOCONFIG)
            endif()
            if(_cdds_loc AND NOT CycloneDDS_LIBRARY)
                set(CycloneDDS_LIBRARY "${_cdds_loc}" CACHE FILEPATH "CycloneDDS library")
            endif()
            if(_cdds_inc AND NOT CycloneDDS_INCLUDE_DIR)
                list(GET _cdds_inc 0 _first_inc)
                set(CycloneDDS_INCLUDE_DIR "${_first_inc}" CACHE PATH "CycloneDDS include dir")
            endif()
        endif()
        # Reset FOUND so our own find_package_handle_standard_args decides
        set(CycloneDDS_FOUND FALSE)
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

    # Always create our own GLOBAL imported target to avoid scope issues
    if(NOT TARGET CycloneDDS::ddsc)
        add_library(CycloneDDS::ddsc UNKNOWN IMPORTED GLOBAL)
        set_target_properties(CycloneDDS::ddsc PROPERTIES
            IMPORTED_LOCATION "${CycloneDDS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CycloneDDS_INCLUDE_DIR}"
        )
    endif()

    message(STATUS "CycloneDDS: ${CycloneDDS_LIBRARY}")
    message(STATUS "CycloneDDS include: ${CycloneDDS_INCLUDE_DIR}")
endif()

mark_as_advanced(CycloneDDS_INCLUDE_DIR CycloneDDS_LIBRARY)
