
SET(HEADER_PATH ${OSG2VSG_SOURCE_DIR}/include/osg2vsg)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/OSG.h
)

set(SOURCES
    convert.cpp
    BuildOptions.cpp
    ConvertToVsg.cpp
    GeometryUtils.cpp
    ImageUtils.cpp
    Optimize.cpp
    OSG.cpp
    SceneAnalysis.cpp
    SceneBuilder.cpp
    ShaderUtils.cpp
)

add_library(osg2vsg ${HEADERS} ${SOURCES})

# add definitions to enable building osg2vsg as part of submodule
add_library(osg2vsg::osg2vsg ALIAS osg2vsg)
set(osg2vsg_FOUND TRUE CACHE INTERNAL "osg2vsg found.")
set(CMAKE_DISABLE_FIND_PACKAGE_osg2vsg TRUE CACHE INTERNAL "Disable find_package(osg2vsg) as it's not necessary.")

set_property(TARGET osg2vsg PROPERTY VERSION ${OSG2VSG_VERSION_MAJOR}.${OSG2VSG_VERSION_MINOR}.${OSG2VSG_VERSION_PATCH})
set_property(TARGET osg2vsg PROPERTY SOVERSION ${OSG2VSG_SOVERSION})
set_property(TARGET osg2vsg PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET osg2vsg PROPERTY CXX_STANDARD 17)

target_include_directories(osg2vsg PUBLIC
    $<BUILD_INTERFACE:${OSG2VSG_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${OSG2VSG_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OSG_INCLUDE_DIR}
)

target_link_libraries(osg2vsg
    PUBLIC
        vsg::vsg
    PRIVATE
        ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES} ${OSGTERRAIN_LIBRARIES}
)


install(TARGETS osg2vsg ${INSTALL_TARGETS_DEFAULT_FLAGS})

target_compile_definitions(osg2vsg PRIVATE ${EXTRA_DEFINES})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(osg2vsg INTERFACE OSG2VSG_SHARED_LIBRARY)
endif()


install(DIRECTORY ${OSG2VSG_SOURCE_DIR}/include/osg2vsg DESTINATION include)
if (NOT(${OSG2VSG_BINARY_DIR} STREQUAL ${OSG2VSG_SOURCE_DIR}))
    # install(DIRECTORY ${OSG2VSG_BINARY_DIR}/include/osg2vsg DESTINATION include)
endif()

vsg_add_cmake_support_files(
    CONFIG_TEMPLATE osg2vsgConfig.cmake
)
