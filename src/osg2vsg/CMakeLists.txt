
SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/osg2vsg)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ImageUtils.h
)

set(SOURCES
    ImageUtils.cpp
)

add_library(osg2vsg ${HEADERS} ${SOURCES})

set_property(TARGET osg2vsg PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET osg2vsg PROPERTY CXX_STANDARD 17)

target_include_directories(osg2vsg PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include ${OSG_INCLUDE_DIR})


target_link_libraries(osg2vsg PUBLIC vsg::vsg Vulkan::Vulkan ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES})

install(TARGETS osg2vsg
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        COMPONENT libosg2vsg
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/osg2vsg DESTINATION include)
