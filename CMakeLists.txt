cmake_minimum_required(VERSION 3.7)

project(osg2vsg
    VERSION 0.1.0
    DESCRIPTION "OpenSceneGraph/VulkanSceneGraph integration library"
    LANGUAGES CXX
)
set(OSG2VSG_SOVERSION 0)
SET(OSG2VSG_RELEASE_CANDIDATE 0)

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

# Find OpenSceneGraph packages for reference
# set prefrence for using modern OpenGL library.
find_package(OpenSceneGraph REQUIRED COMPONENTS osgDB osgTerrain osgUtil osgGA osgViewer)

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(vsg 1.0.3)

vsg_setup_dir_vars()
vsg_setup_build_vars()

vsg_add_target_clang_format(
    FILES
        ${PROJECT_SOURCE_DIR}/include/*/*.h
        ${PROJECT_SOURCE_DIR}/src/*/*.cpp
)
vsg_add_target_clobber()
vsg_add_target_cppcheck(
    FILES
        ${PROJECT_SOURCE_DIR}/include/*/*.h
        ${PROJECT_SOURCE_DIR}/src/*/*.cpp
        -I ${PROJECT_SOURCE_DIR}/include/
)
vsg_add_target_docs(
    FILES
        ${PROJECT_SOURCE_DIR}/include/
)
vsg_add_target_uninstall()


# only provide custom targets if not building as a submodule/FetchContent
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    vsg_add_option_maintainer(
        PREFIX v
        RCLEVEL ${OSG2VSG_RELEASE_CANDIDATE}
    )

endif()

# src directory contains all the example applications etc.
add_subdirectory(src/osg2vsg)
add_subdirectory(src/osgPlugins/vsg)
add_subdirectory(applications)
