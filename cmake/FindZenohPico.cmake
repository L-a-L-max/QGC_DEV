# FindZenohPico.cmake
#
# Find the Eclipse zenoh-pico library (libzenohpico).
#
# Sets:
#   ZenohPico_FOUND        - TRUE if zenoh-pico is found
#   ZenohPico_INCLUDE_DIRS - Include directories
#   ZenohPico_LIBRARIES    - Libraries to link against
#
# Creates imported target:
#   ZenohPico::zenohpico   - The zenoh-pico library (GLOBAL)
#
# Supports:
#   1. ZENOHPICO_ROOT cmake var / ZENOHPICO_HOME env (highest priority)
#   2. Manual search: /usr/local, /usr, custom paths
#   3. pkg-config as fallback

# Candidate prefixes
set(_ZP_SEARCH_PREFIXES "")

if(ZENOHPICO_ROOT)
    list(APPEND _ZP_SEARCH_PREFIXES "${ZENOHPICO_ROOT}")
endif()
if(DEFINED ENV{ZENOHPICO_HOME} AND NOT "$ENV{ZENOHPICO_HOME}" STREQUAL "")
    list(APPEND _ZP_SEARCH_PREFIXES "$ENV{ZENOHPICO_HOME}")
endif()

list(APPEND _ZP_SEARCH_PREFIXES
    /usr/local
    /usr
    /opt/zenoh-pico
)

# Try each prefix
foreach(_prefix IN LISTS _ZP_SEARCH_PREFIXES)
    if(ZenohPico_LIBRARY AND ZenohPico_INCLUDE_DIR)
        break()
    endif()

    find_path(_zp_inc_candidate
        NAMES zenoh-pico.h
        PATHS "${_prefix}/include"
        NO_DEFAULT_PATH
    )

    find_library(_zp_lib_candidate
        NAMES zenohpico
        PATHS "${_prefix}/lib" "${_prefix}/lib/x86_64-linux-gnu"
        NO_DEFAULT_PATH
    )

    if(_zp_inc_candidate AND _zp_lib_candidate)
        set(ZenohPico_INCLUDE_DIR "${_zp_inc_candidate}" CACHE PATH "zenoh-pico include dir")
        set(ZenohPico_LIBRARY "${_zp_lib_candidate}" CACHE FILEPATH "zenoh-pico library")
        message(STATUS "zenoh-pico found in prefix: ${_prefix}")
    endif()

    unset(_zp_inc_candidate CACHE)
    unset(_zp_lib_candidate CACHE)
endforeach()

# Fallback: pkg-config
if(NOT ZenohPico_LIBRARY OR NOT ZenohPico_INCLUDE_DIR)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(_ZP QUIET zenohpico)
        if(_ZP_FOUND)
            if(NOT ZenohPico_INCLUDE_DIR)
                set(ZenohPico_INCLUDE_DIR "${_ZP_INCLUDE_DIRS}" CACHE PATH "zenoh-pico include dir")
            endif()
            if(NOT ZenohPico_LIBRARY)
                find_library(ZenohPico_LIBRARY NAMES zenohpico HINTS ${_ZP_LIBRARY_DIRS})
            endif()
        endif()
    endif()
endif()

# Last-resort relaxed search
if(NOT ZenohPico_INCLUDE_DIR)
    find_path(ZenohPico_INCLUDE_DIR NAMES zenoh-pico.h)
endif()
if(NOT ZenohPico_LIBRARY)
    find_library(ZenohPico_LIBRARY NAMES zenohpico)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZenohPico
    REQUIRED_VARS ZenohPico_LIBRARY ZenohPico_INCLUDE_DIR
)

if(ZenohPico_FOUND)
    set(ZenohPico_LIBRARIES ${ZenohPico_LIBRARY})
    set(ZenohPico_INCLUDE_DIRS ${ZenohPico_INCLUDE_DIR})

    if(NOT TARGET ZenohPico::zenohpico)
        add_library(ZenohPico::zenohpico UNKNOWN IMPORTED GLOBAL)
        set_target_properties(ZenohPico::zenohpico PROPERTIES
            IMPORTED_LOCATION "${ZenohPico_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZenohPico_INCLUDE_DIR}"
        )
    endif()

    message(STATUS "zenoh-pico: ${ZenohPico_LIBRARY}")
    message(STATUS "zenoh-pico include: ${ZenohPico_INCLUDE_DIR}")
endif()

mark_as_advanced(ZenohPico_INCLUDE_DIR ZenohPico_LIBRARY)
