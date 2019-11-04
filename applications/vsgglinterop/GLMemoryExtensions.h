/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#pragma once

#include <osg/GLExtensions>


#ifndef GL_EXT_memory_object
#define GL_TEXTURE_TILING_EXT             0x9580
#define GL_DEDICATED_MEMORY_OBJECT_EXT    0x9581
#define GL_PROTECTED_MEMORY_OBJECT_EXT    0x959B
#define GL_NUM_TILING_TYPES_EXT           0x9582
#define GL_TILING_TYPES_EXT               0x9583
#define GL_OPTIMAL_TILING_EXT             0x9584
#define GL_LINEAR_TILING_EXT              0x9585
#endif

#if WIN32
#ifndef GL_EXT_memory_object_win32
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT   0x9587
#endif
#else
#ifndef GL_EXT_memory_object_fd
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT      0x9586
#endif
#endif

#ifndef GL_EXT_semaphore
#define GL_LAYOUT_GENERAL_EXT             0x958D
#define GL_LAYOUT_COLOR_ATTACHMENT_EXT    0x958E
#define GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT 0x958F
#define GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT 0x9590
#define GL_LAYOUT_SHADER_READ_ONLY_EXT    0x9591
#define GL_LAYOUT_TRANSFER_SRC_EXT        0x9592
#define GL_LAYOUT_TRANSFER_DST_EXT        0x9593
#define GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT 0x9530
#define GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT 0x9531
#endif

namespace osg
{

    class GLMemoryExtensions : public osg::Referenced
    {
    public:
        static GLMemoryExtensions* Get(unsigned int contextID, bool createIfNotInitalized);

        GLMemoryExtensions(unsigned int in_contextID);

        void (GL_APIENTRY * glGetInternalformativ) (GLenum, GLenum, GLenum, GLsizei, GLint*);

        void (GL_APIENTRY * glCreateTextures) (GLenum, GLsizei, GLuint*);
        void (GL_APIENTRY * glTextureParameteri) (GLenum, GLenum, GLint);

        void (GL_APIENTRY * glCopyImageSubData) (GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);

        // memory objects
        void (GL_APIENTRY * glCreateMemoryObjectsEXT) (GLsizei, GLuint*);
        void (GL_APIENTRY * glMemoryObjectParameterivEXT) (GLuint, GLenum, const GLint*);
    #if WIN32
        void (GL_APIENTRY * glImportMemoryWin32HandleEXT) (GLuint, GLuint64, GLenum, void*);
    #else
        void (GL_APIENTRY * glImportMemoryFdEXT) (GLuint, GLuint64, GLenum, GLint);
    #endif

        void (GL_APIENTRY * glTextureStorageMem2DEXT) (GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLuint, GLuint64);

        // semaphores
        void (GL_APIENTRY * glGenSemaphoresEXT) (GLsizei, GLuint*);
        void (GL_APIENTRY * glDeleteSemaphoresEXT) (GLsizei, const GLuint*);
        GLboolean (GL_APIENTRY * glIsSemaphoreEXT) (GLuint);
        void (GL_APIENTRY * glSemaphoreParameterui64vEXT) (GLuint, GLenum, const GLuint64*);
        void (GL_APIENTRY * glGetSemaphoreParameterui64vEXT) (GLuint, GLenum, GLuint64*);
        void (GL_APIENTRY * glWaitSemaphoreEXT) (GLuint, GLuint, const GLuint*, GLuint, const GLuint*, const GLenum*);
        void (GL_APIENTRY * glSignalSemaphoreEXT) (GLuint, GLuint, const GLuint*, GLuint, const GLuint*, const GLenum*);

#if WIN32
        void (GL_APIENTRY * glImportSemaphoreWin32HandleEXT) (GLuint, GLenum, void*);
#else
        void (GL_APIENTRY * glImportSemaphoreFdEXT) (GLuint, GLenum, GLint);
#endif
    };
}

