set(SOURCES
    ReaderWriterVSG.cpp
)

add_library(osgdb_vsg MODULE ${SOURCES})

set(OSG_PLUGINS osgPlugins)

set_target_properties(osgdb_vsg PROPERTIES PREFIX "${OSG_PLUGINS_DIRECTORY}")

set_property(TARGET osgdb_vsg PROPERTY CXX_STANDARD 17)

target_include_directories(osgdb_vsg PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${OSG_INCLUDE_DIR}
)

target_link_libraries(osgdb_vsg PUBLIC
    vsg::vsg
    Vulkan::Vulkan
    ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES}
)
