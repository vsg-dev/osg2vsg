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
find_package(OpenSceneGraph REQUIRED
    COMPONENTS
        osgDB osgTerrain osgUtil osgGA osgViewer
)

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(vsg 1.0.0)

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
if (${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    option(MAINTAINER "Enable maintainer build methods, such as making git branches and tags." OFF)
    if (MAINTAINER)

        #
        # Provide target for tagging a release
        #
        SET(BASE_NAME osg2vsg)
        SET(OSG2VSG_BRANCH ${BASE_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

        SET(GITCOMMAND git)

        IF   (OSG2VSG_RELEASE_CANDIDATE EQUAL 0)
            SET(RELEASE_NAME ${BASE_NAME}-${PROJECT_VERSION})
        ELSE()
            SET(RELEASE_NAME ${BASE_NAME}-${PROJECT_VERSION}-rc${OSG2VSG_RELEASE_CANDIDATE})
        ENDIF()

        SET(RELEASE_MESSAGE "Release ${RELEASE_NAME}")
        SET(BRANCH_MESSAGE "Branch ${OSG2VSG_BRANCH}")

        add_custom_target(tag-test
            COMMAND echo ${GITCOMMAND} tag -a ${RELEASE_NAME} -m ${RELEASE_MESSAGE}
            COMMAND echo ${GITCOMMAND} push origin ${RELEASE_NAME}
        )

        add_custom_target(tag-run
            COMMAND ${GITCOMMAND} tag -a ${RELEASE_NAME} -m ${RELEASE_MESSAGE}
            COMMAND ${GITCOMMAND} push origin ${RELEASE_NAME}
        )

        add_custom_target(branch-test
            COMMAND echo ${GITCOMMAND} branch ${OSG2VSG_BRANCH}
            COMMAND echo ${GITCOMMAND} push origin ${OSG2VSG_BRANCH}
        )

        add_custom_target(branch-run
            COMMAND ${GITCOMMAND} branch ${OSG2VSG_BRANCH}
            COMMAND ${GITCOMMAND} push origin ${OSG2VSG_BRANCH}
        )

    endif()
endif()

# src directory contains all the example applications etc.
add_subdirectory(src/osg2vsg)
add_subdirectory(src/osgPlugins/vsg)
add_subdirectory(applications)
