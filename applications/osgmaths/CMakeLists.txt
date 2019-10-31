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

set(SOURCES osgmaths.cpp)

add_executable(osgmaths ${SOURCES})
target_include_directories(osgmaths PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(osgmaths
    vsg::vsg
    ${OSGDB_LIBRARIES} ${OSG_LIBRARIES} ${OPENTHREADS_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_LIBRARY} ${DL_LIBRARY}
)

