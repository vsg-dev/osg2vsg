
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${OSG_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIR}
    ${VSG_INCLUDE_DIR}
)

SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/osg2vsg)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ImageUtils.h
)

set(SOURCES
    ImageUtils.cpp
)

add_library(osg2vsg ${HEADERS} ${SOURCES})

target_link_libraries(osg2vsg VSG::VSG ${Vulkan_LIBRARIES}  ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES})

install(TARGETS osg2vsg
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        COMPONENT libosg2vsg
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/osg2vsg DESTINATION include)
