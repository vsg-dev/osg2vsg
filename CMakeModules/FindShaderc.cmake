#.rst:
# FindShaderc
# ----------
#
# Try to find Shaderc in the VulkanSDK
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``Shaderc::Shaderc``, if
# Shaderc has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables::
#
#   Shaderc_FOUND          - True if Shaderc was found
#   Shaderc_INCLUDE_DIRS   - include directories for Shaderc
#   Shaderc_LIBRARIES      - link against this library to use Shaderc
#
# The module will also define two cache variables::
#
#   Shaderc_INCLUDE_DIR    - the Shaderc include directory
#   Shaderc_LIBRARY        - the path to the Shaderc library
#


if(WIN32)
  find_path(Shaderc_INCLUDE_DIR
    NAMES shaderc/shaderc.h
    PATHS
      "$ENV{VULKAN_SDK}/Include"
    )

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library(Shaderc_LIBRARY
      NAMES shaderc_combined
      PATHS
        "$ENV{VULKAN_SDK}/Lib"
        )
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    find_library(Shaderc_LIBRARY
      NAMES shaderc_combined
      PATHS
        "$ENV{VULKAN_SDK}/Lib32"
        )
  endif()
else()
    find_path(Shaderc_INCLUDE_DIR
      NAMES shaderc/shaderc.h
      PATHS
        "$ENV{VULKAN_SDK}/include")
    find_library(Shaderc_LIBRARY
      NAMES shaderc_combined
      PATHS
        "$ENV{VULKAN_SDK}/lib")
endif()

set(Shaderc_LIBRARIES ${Shaderc_LIBRARY})
set(Shaderc_INCLUDE_DIRS ${Shaderc_INCLUDE_DIR})

mark_as_advanced(Shaderc_INCLUDE_DIR Shaderc_LIBRARY)

if(Shaderc_LIBRARY AND Shaderc_INCLUDE_DIR)
    set(Shaderc_FOUND "YES")
    message(STATUS "Found Shaderc: ${Shaderc_LIBRARY}")
else()
    set(Shaderc_FOUND "NO")
    message(STATUS "Failed to find Shaderc")
endif()

if(Shaderc_FOUND AND NOT TARGET Shaderc::Shaderc)
  add_library(Shaderc::Shaderc UNKNOWN IMPORTED)
  set_target_properties(Shaderc::Shaderc PROPERTIES
    IMPORTED_LOCATION "${Shaderc_LIBRARIES}"
    INTERFACE_INCLUDE_DIRECTORIES "${Shaderc_INCLUDE_DIRS}")
endif()
