
INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/include
    ${OSG_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIR}
    ${VSG_INCLUDE_DIR}
)

SET(HEADERS
    Export.h
    ImageUtils.h
)

SET(SOURCES
    ImageUtils.cpp
)

if   (VSG_SHARED_LIBRARY)
    add_definitions(-DVSG_LIBRARY)
    add_library(osg2vsg SHARED ${HEADER_FILES} ${SOURCES})
else ()
    add_definitions(-DVSG_LIBRARY_STATIC)
    add_library(osg2vsg STATIC ${HEADER_FILES} ${SOURCES})
endif()

TARGET_LINK_LIBRARIES(osg2vsg ${Vulkan_LIBRARY} ${Vulkan_LIBRARIES}  ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES})

install(TARGETS osg2vsg
        LIBRARY
            DESTINATION lib
            COMPONENT libosg2vsg
)


install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/osg2vsg DESTINATION include)

