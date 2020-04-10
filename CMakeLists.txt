cmake_minimum_required(VERSION 3.7)

project(OSG2VSG
    VERSION 0.0.0
    DESCRIPTION "OpenSceneGraph/Vulkan/VkSceneGraph integration library"
    LANGUAGES CXX
)
set(OSG2VSG_SOVERSION 0)

set(OUTPUT_BINDIR ${PROJECT_BINARY_DIR}/bin)
set(OUTPUT_LIBDIR ${PROJECT_BINARY_DIR}/lib)

#set(OSG_PLUGINS_DIRECTORY osgPlugins-$OSG_MAJOR_VERSION}.$OSG_MINOR_VERSION}.${OSG_PATCH_VERSION})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
SET(CMAKE_MODULE_PATH "${OSG2VSG_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
SET(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_RELWITHDEBINFO_POSTFIX "rd" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_MINSIZEREL_POSTFIX "s" CACHE STRING "add a postfix, usually empty on windows")

if(WIN32)
    SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
endif(WIN32)

# set up local bin directory to place all binaries
if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
    # set up local bin directory to place all binaries
    make_directory(${OUTPUT_BINDIR})
    make_directory(${OUTPUT_LIBDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
    # set up local bin directory to place all binaries
    make_directory(${OUTPUT_LIBDIR})
endif(WIN32)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})


# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wmaybe-uninitialized -Wshadow -Wunused -Wno-misleading-indentation -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wuninitialized -Wunused -Wextra -Wno-overloaded-virtual)
endif()

set(VSG_WARNING_FLAGS ${VSG_WARNING_FLAGS} CACHE STRING "Compiler flags to use." FORCE)


foreach (flag ${VSG_WARNING_FLAGS})
    if(NOT CMAKE_CXX_FLAGS MATCHES "${flag}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endforeach ()


# Find OpenSceneGraph packages for reference
find_package(OpenThreads REQUIRED)
find_package(osg REQUIRED)
find_package(osgDB REQUIRED)
find_package(osgTerrain REQUIRED)
find_package(osgUtil REQUIRED)
find_package(OpenGL REQUIRED)

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)
find_package(vsg REQUIRED)


add_custom_target(clobber
    COMMAND git clean -d -f -x
)

# automatically buil_all_h build target to generate the include/vsg/all.h from the headers in the include/vsg/* directories
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/build/uninstall.cmake
)

# src directory contains all the example applications etc.
add_subdirectory(src/osg2vsg)
add_subdirectory(src/osgPlugins/vsg)
add_subdirectory(applications)
