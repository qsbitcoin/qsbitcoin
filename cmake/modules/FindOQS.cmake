# FindOQS.cmake - Find the Open Quantum Safe (liboqs) library
#
# This module defines:
#  OQS_FOUND - System has liboqs
#  OQS_INCLUDE_DIRS - The liboqs include directories
#  OQS_LIBRARIES - The libraries needed to use liboqs
#  OQS::OQS - Imported target

find_path(OQS_INCLUDE_DIR
    NAMES oqs/oqs.h
    PATHS ${CMAKE_SOURCE_DIR}/liboqs/include
          /usr/include
          /usr/local/include
)

find_library(OQS_LIBRARY
    NAMES oqs
    PATHS ${CMAKE_BINARY_DIR}/liboqs/lib
          /usr/lib
          /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OQS
    REQUIRED_VARS OQS_LIBRARY OQS_INCLUDE_DIR
)

if(OQS_FOUND)
    set(OQS_LIBRARIES ${OQS_LIBRARY})
    set(OQS_INCLUDE_DIRS ${OQS_INCLUDE_DIR})
    
    if(NOT TARGET OQS::OQS)
        add_library(OQS::OQS UNKNOWN IMPORTED)
        set_target_properties(OQS::OQS PROPERTIES
            IMPORTED_LOCATION "${OQS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OQS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(OQS_INCLUDE_DIR OQS_LIBRARY)