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
#   CycloneDDS::ddsc       - The CycloneDDS C library (GLOBAL)
#
# Supports:
#   1. CYCLONEDDS_ROOT cmake var / CYCLONEDDS_HOME env (highest priority)
#   2. Manual search: /usr/local first, then /usr, then /opt paths
#   3. pkg-config as fallback hint
#
# NOTE: We do NOT call find_package(CycloneDDS CONFIG) here because:
#   - CONFIG creates IMPORTED targets whose scope does not survive CMake's
#     generate phase when invoked from within a Find module (CMake 4.x).
#   - On systems with multiple CycloneDDS installs (e.g. /usr/local + ROS 2)
#     CONFIG may mix paths from different prefixes.
# Instead we locate the library and headers ourselves, verify they come from
# the same install prefix, and create a single GLOBAL IMPORTED target.

# ── Collect candidate prefixes (order = priority) ──────────────────────
set(_CDDS_SEARCH_PREFIXES "")

# User-specified paths have highest priority
if(CYCLONEDDS_ROOT)
    list(APPEND _CDDS_SEARCH_PREFIXES "${CYCLONEDDS_ROOT}")
endif()
if(DEFINED ENV{CYCLONEDDS_HOME} AND NOT "$ENV{CYCLONEDDS_HOME}" STREQUAL "")
    list(APPEND _CDDS_SEARCH_PREFIXES "$ENV{CYCLONEDDS_HOME}")
endif()

# Standard system prefixes — /usr/local before /usr before ROS
list(APPEND _CDDS_SEARCH_PREFIXES
    /usr/local
    /usr
    /opt/cyclonedds
    /opt/ros/humble
    /opt/ros/iron
    /opt/ros/jazzy
)

# ── Try each prefix: find both header AND library in the same prefix ───
foreach(_prefix IN LISTS _CDDS_SEARCH_PREFIXES)
    if(CycloneDDS_LIBRARY AND CycloneDDS_INCLUDE_DIR)
        break()
    endif()

    # Look for the header
    find_path(_cdds_inc_candidate
        NAMES dds/dds.h
        PATHS "${_prefix}/include"
        PATH_SUFFIXES cyclonedds
        NO_DEFAULT_PATH
    )

    # Look for the library
    find_library(_cdds_lib_candidate
        NAMES ddsc
        PATHS "${_prefix}/lib" "${_prefix}/lib/x86_64-linux-gnu"
        NO_DEFAULT_PATH
    )

    if(_cdds_inc_candidate AND _cdds_lib_candidate)
        set(CycloneDDS_INCLUDE_DIR "${_cdds_inc_candidate}" CACHE PATH "CycloneDDS include dir")
        set(CycloneDDS_LIBRARY "${_cdds_lib_candidate}" CACHE FILEPATH "CycloneDDS library")
        message(STATUS "CycloneDDS found in prefix: ${_prefix}")
    endif()

    # Reset candidates for next iteration
    unset(_cdds_inc_candidate CACHE)
    unset(_cdds_lib_candidate CACHE)
endforeach()

# ── Fallback: pkg-config (if prefix search found nothing) ─────────────
if(NOT CycloneDDS_LIBRARY OR NOT CycloneDDS_INCLUDE_DIR)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(_CDDS QUIET cyclonedds)
        if(_CDDS_FOUND)
            if(NOT CycloneDDS_INCLUDE_DIR)
                set(CycloneDDS_INCLUDE_DIR "${_CDDS_INCLUDE_DIRS}" CACHE PATH "CycloneDDS include dir")
            endif()
            if(NOT CycloneDDS_LIBRARY)
                find_library(CycloneDDS_LIBRARY
                    NAMES ddsc
                    HINTS ${_CDDS_LIBRARY_DIRS}
                )
            endif()
            if(NOT CycloneDDS_VERSION)
                set(CycloneDDS_VERSION "${_CDDS_VERSION}")
            endif()
        endif()
    endif()
endif()

# ── Last-resort: relaxed search (let CMake search default paths) ──────
if(NOT CycloneDDS_INCLUDE_DIR)
    find_path(CycloneDDS_INCLUDE_DIR NAMES dds/dds.h PATH_SUFFIXES cyclonedds)
endif()
if(NOT CycloneDDS_LIBRARY)
    find_library(CycloneDDS_LIBRARY NAMES ddsc)
endif()

# ── Standard result handling ──────────────────────────────────────────
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CycloneDDS
    REQUIRED_VARS CycloneDDS_LIBRARY CycloneDDS_INCLUDE_DIR
)

if(CycloneDDS_FOUND)
    set(CycloneDDS_LIBRARIES ${CycloneDDS_LIBRARY})
    set(CycloneDDS_INCLUDE_DIRS ${CycloneDDS_INCLUDE_DIR})

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
