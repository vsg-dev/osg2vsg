
SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/osg2vsg)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/OSG.h
)

set(SOURCES
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

# check whehther glslang/build_info.h exists
if (EXISTS ${glslang_INCLUDE_DIR}/glslang/build_info.h)
    set(EXTRA_DEFINES ${EXTRA_DEFINES} GLSLANG_HAS_BUILD_INFO_H)
endif()

set_property(TARGET osg2vsg PROPERTY VERSION ${OSG2VSG_VERSION_MAJOR}.${OSG2VSG_VERSION_MINOR}.${OSG2VSG_VERSION_PATCH})
set_property(TARGET osg2vsg PROPERTY SOVERSION ${OSG2VSG_SOVERSION})
set_property(TARGET osg2vsg PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET osg2vsg PROPERTY CXX_STANDARD 17)

target_include_directories(osg2vsg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OSG_INCLUDE_DIR}
)

target_link_libraries(osg2vsg
    PUBLIC
        vsg::vsg
    PRIVATE
        ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES} ${OSGTERRAIN_LIBRARIES}
)


install(TARGETS osg2vsg EXPORT osg2vsgTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

target_compile_definitions(osg2vsg PRIVATE ${EXTRA_DEFINES})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(osg2vsg INTERFACE OSG2VSG_SHARED_LIBRARY)
endif()


install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/osg2vsg DESTINATION include)

# [==[
install(EXPORT osg2vsgTargets
    FILE osg2vsgTargets.cmake
    NAMESPACE osg2vsg::
    DESTINATION lib/cmake/osg2vsg
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/src/osg2vsg/osg2vsgConfigVersion.cmake"
    VERSION ${OSG2VSG_MAJOR_VERSION}.${OSG2VSG_MINOR_VERSION}.${OSG2VSG_PATCH_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "osg2vsgConfig.cmake" "${CMAKE_BINARY_DIR}/src/osg2vsg/osg2vsgConfigVersion.cmake"
    DESTINATION lib/cmake/osg2vsg
)

# ]==]
