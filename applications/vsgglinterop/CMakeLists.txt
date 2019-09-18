find_package(OpenGL)

if(WIN32)
    set(OPENGL_LIBRARY ${OPENGL_gl_LIBRARY})
else()
    set(OPENGL_LIBRARY OpenGL::GL)
endif()

set(SOURCES vsgglinterop.cpp 
            SharedImage.h SharedImage.cpp
            SharedSemaphore.h SharedSemaphore.cpp
            SharedTexture.h SharedTexture.cpp
            GLSemaphore.h GLSemaphore.cpp
            SyncTextureDrawCallbacks.h
            GLMemoryExtensions.h GLMemoryExtensions.cpp)

add_executable(vsgglinterop ${SOURCES})
target_include_directories(vsgglinterop PRIVATE ${OSG_INCLUDE_DIR})
target_link_libraries(vsgglinterop Vulkan::Vulkan vsg::vsg ${OPENGL_LIBRARY} ${OSG_LIBRARIES} ${OSGGA_LIBRARY} ${OSGVIEWER_LIBRARY} osg2vsg ${GLSLANG})
