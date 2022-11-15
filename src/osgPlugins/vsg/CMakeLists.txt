set(SOURCES
    ReaderWriterVSG.cpp
)

add_library(osgdb_vsg MODULE ${SOURCES})

set(OSG_PLUGINS osgPlugins)

set_target_properties(osgdb_vsg PROPERTIES PREFIX "${OSG_PLUGINS_DIRECTORY}")

set_property(TARGET osgdb_vsg PROPERTY CXX_STANDARD 17)

target_include_directories(osgdb_vsg PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    ${OSG_INCLUDE_DIR}
)

target_link_libraries(osgdb_vsg PUBLIC
    osg2vsg
    vsg::vsg
    ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES}
)

install(TARGETS osgdb_vsg ${INSTALL_TARGETS_DEFAULT_FLAGS})
