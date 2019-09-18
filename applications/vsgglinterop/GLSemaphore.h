/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#pragma once

#include <vector>

#include <osg/GL>
#include <osg/Referenced>

#if WIN32
#include <Windows.h>
#endif

namespace osg
{
    class GLSemaphore : public osg::Referenced
    {
    public:
#if WIN32
        typedef HANDLE HandleType;
#else
        typedef int HandleType;
#endif

        GLSemaphore(HandleType handle);

        void compileGLObjects(State& state);

        void wait(State& state, const std::vector<GLuint>& buffers, const std::vector<GLuint>& textures, const std::vector<GLenum>& srcLayouts);
        void wait(State& state, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts);

        void signal(State& state, const std::vector<GLuint>& buffers, const std::vector<GLuint>& textures, const std::vector<GLenum>& srcLayouts);
        void signal(State& state, GLuint numBufferBarriers, const GLuint *buffers, GLuint numTextureBarriers, const GLuint *textures, const GLenum *srcLayouts);

    protected:
        ~GLSemaphore();

        HandleType _handle;
        GLuint _semaphore;
    };
}

