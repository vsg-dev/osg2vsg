find_package(OpenGL)

if(WIN32)
    set(OPENGL_LIBRARY ${OPENGL_gl_LIBRARY})
else()
    set(OPENGL_LIBRARY OpenGL::GL)
endif()

if(NOT ANDROID)
    find_package(Threads)
endif()

if (UNIX)
    find_library(DL_LIBRARY dl)
endif()

set(SOURCES
    ConvertToVsg.cpp
    pdconv.cpp)

add_executable(pdconv ${SOURCES})

target_include_directories(pdconv PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    ${OSG_INCLUDE_DIR}
)

set_target_properties(pdconv PROPERTIES OUTPUT_NAME pdconv)

target_link_libraries(pdconv
    osg2vsg
    vsg::vsg
    ${GLSLANG}
    Vulkan::Vulkan
    ${OSGTERRAIN_LIBRARIES} ${OSGDB_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSG_LIBRARIES} ${OPENTHREADS_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_LIBRARY} ${DL_LIBRARY}
)


install(TARGETS pdconv
        RUNTIME DESTINATION bin
)
