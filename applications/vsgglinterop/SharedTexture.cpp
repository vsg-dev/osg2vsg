#include "SharedTexture.h"

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
        textureObject->bind(state);
    }
    else
    {
        osg::GLExtensions * extensions = state.get<osg::GLExtensions>();

        // compute the internal texture format, this set the _internalFormat to an appropriate value.
        computeInternalFormat();

        GLenum texStorageSizedInternalFormat = selectSizedInternalFormat(NULL);

        // this could potentially return a recycled texture object which we don't want!!!
        textureObject = generateAndAssignTextureObject(contextID, GL_TEXTURE_2D, 1,
            texStorageSizedInternalFormat != 0 ? texStorageSizedInternalFormat : _internalFormat,
            _textureWidth, _textureHeight, 1, 0);

        textureObject->bind(state);

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
