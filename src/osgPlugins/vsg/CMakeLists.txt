set(SOURCES
    ReaderWriterVSG.cpp
)

add_library(osgdb_vsg MODULE ${SOURCES})
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

# osg plugins related
set(OSG_PLUGINS_DIRECTORY osgPlugins-${OPENSCENEGRAPH_VERSION})
if(CYGWIN)
    set(OSG_PLUGIN_PREFIX "cygwin_")
elseif(MINGW)
    set(OSG_PLUGIN_PREFIX "mingw_")
else()
    set(OSG_PLUGIN_PREFIX "")
endif()
set(CMAKE_SHARED_MODULE_PREFIX ${OSG_PLUGIN_PREFIX})

# do not use INSTALL_TARGETS_DEFAULT_FLAGS, it does not match
install(TARGETS osgdb_vsg EXPORT osg2vsgTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${OSG_PLUGINS_DIRECTORY}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/${OSG_PLUGINS_DIRECTORY}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
