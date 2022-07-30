/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "SharedTexture.h"

#include "GLMemoryExtensions.h"

#include <osg/Version>

using namespace osg;

SharedTexture::SharedTexture() :
    Texture(),
    _byteSize(0),
    _textureWidth(0),
    _textureHeight(0),
    _handle(0),
    _tiling(0),
    _isDedicated(false)
{
    setUseHardwareMipMapGeneration(false);
    setResizeNonPowerOfTwoHint(false);
}

SharedTexture::SharedTexture(HandleType handle, GLuint64 byteSize, int width, int height, GLenum internalFormat, GLuint tiling, bool isDedicated) :
    Texture(),
    _byteSize(byteSize),
    _textureWidth(width),
    _textureHeight(height),
    _handle(handle),
    _tiling(tiling),
    _isDedicated(isDedicated)
{
    setInternalFormat(internalFormat);
    setUseHardwareMipMapGeneration(false);
    setResizeNonPowerOfTwoHint(false);
}


SharedTexture::~SharedTexture()
{
}

void SharedTexture::apply(State& state) const
{
    const unsigned int contextID = state.getContextID();

    // get the texture object for the current contextID.
    TextureObject* textureObject = getTextureObject(contextID);

    if (textureObject)
    {
#if OSG_VERSION_GREATER_OR_EQUAL(3, 7, 0)
        textureObject->bind(state);
#else
        textureObject->bind();
#endif
    }
    else
    {
        const osg::GLMemoryExtensions * extensions = GLMemoryExtensions::Get(state.getContextID(), true);
        if (!extensions->glCreateMemoryObjectsEXT) return;

        // compute the internal texture format, this set the _internalFormat to an appropriate value.
        computeInternalFormat();

        GLenum texStorageSizedInternalFormat = (GLenum)_internalFormat; //selectSizedInternalFormat(NULL);

        // this could potentially return a recycled texture object which we don't want!!!
        textureObject = generateAndAssignTextureObject(contextID, GL_TEXTURE_2D, 1,
            texStorageSizedInternalFormat != 0 ? texStorageSizedInternalFormat : _internalFormat,
            _textureWidth, _textureHeight, 1, 0);

#if OSG_VERSION_GREATER_OR_EQUAL(3, 7, 0)
        textureObject->bind(state);
#else
        textureObject->bind();
#endif

        applyTexParameters(GL_TEXTURE_2D, state);

        // create memory object
        GLuint memory = 0;
        extensions->glCreateMemoryObjectsEXT(1, &memory);
        if (_isDedicated)
        {
            static const GLint DEDICATED_FLAG = GL_TRUE;
            extensions->glMemoryObjectParameterivEXT(memory, GL_DEDICATED_MEMORY_OBJECT_EXT, &DEDICATED_FLAG);
        }

#if WIN32
        extensions->glImportMemoryWin32HandleEXT(memory, _byteSize, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle);
#else
        extensions->glImportMemoryFdEXT(memory, _byteSize, GL_HANDLE_TYPE_OPAQUE_FD_EXT, _handle);
#endif

        // create texture storage using the memory
        extensions->glTextureParameteri(textureObject->id(), GL_TEXTURE_TILING_EXT, _tiling);
        extensions->glTextureStorageMem2DEXT(textureObject->id(), 1, texStorageSizedInternalFormat, _textureWidth, _textureHeight, memory, 0);

        osg::ref_ptr<osg::Image> image = _image;
        if(image.valid())
        {
            unsigned char* dataPtr = (unsigned char*)image->data();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                _textureWidth, _textureHeight,
                (GLenum)image->getPixelFormat(),
                (GLenum)image->getDataType(),
                dataPtr);
        }

        textureObject->setAllocated(true);
    }
}

std::vector<GLint> SharedTexture::getSupportedTilingTypesForFormat(const osg::State& state, GLenum format)
{
    std::vector<GLint> results;

    const osg::GLMemoryExtensions* extensions = osg::GLMemoryExtensions::Get(state.getContextID(), true);

    GLint numTilingTypes = 0;
    extensions->glGetInternalformativ(GL_TEXTURE_2D, format, GL_NUM_TILING_TYPES_EXT, 1, &numTilingTypes);

    // Broken tiling detection on AMD
    if (numTilingTypes == 0)
    {
        results.push_back(GL_LINEAR_TILING_EXT);
        return results;
    }

    results.resize(numTilingTypes);
    extensions->glGetInternalformativ(GL_TEXTURE_2D, format, GL_TILING_TYPES_EXT, numTilingTypes, results.data());

    return results;
}
