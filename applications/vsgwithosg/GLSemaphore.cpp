/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "GLSemaphore.h"

#include "GLMemoryExtensions.h"

#include <osg/State>

using namespace osg;

GLSemaphore::GLSemaphore(HandleType handle) :
    Referenced(),
    _handle(handle),
    _semaphore(0)
{
}


GLSemaphore::~GLSemaphore()
{
}

void GLSemaphore::compileGLObjects(State& state)
{
    if(_handle == 0 || _semaphore != 0) return;

    GLMemoryExtensions* extensions = GLMemoryExtensions::Get(state.getContextID(), true);
    if(!extensions->glGenSemaphoresEXT) return;

    extensions->glGenSemaphoresEXT(1, &_semaphore);
#if WIN32
    extensions->glImportSemaphoreWin32HandleEXT(_semaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle);
#else
    extensions->glImportSemaphoreFdEXT(_semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, _handle);
#endif
}

void GLSemaphore::wait(State& state, const std::vector<GLuint>& buffers, const std::vector<GLuint>& textures, const std::vector<GLenum>& srcLayouts)
{
    wait(state, buffers.size(), buffers.data(), textures.size(), textures.data(), srcLayouts.data());
}

void GLSemaphore::wait(State& state, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts)
{
    GLMemoryExtensions* extensions = GLMemoryExtensions::Get(state.getContextID(), true);

    extensions->glWaitSemaphoreEXT(_semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
}

void GLSemaphore::signal(State& state, const std::vector<GLuint>& buffers, const std::vector<GLuint>& textures, const std::vector<GLenum>& srcLayouts)
{
    signal(state, buffers.size(), buffers.data(), textures.size(), textures.data(), srcLayouts.data());
}

void GLSemaphore::signal(State& state, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts)
{
    GLMemoryExtensions* extensions = GLMemoryExtensions::Get(state.getContextID(), true);

    extensions->glSignalSemaphoreEXT(_semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
}
