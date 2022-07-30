#include "GLMemoryExtensions.h"

#include <osg/buffered_value>

using namespace osg;

typedef osg::buffered_object< osg::ref_ptr<GLMemoryExtensions> > BufferedMemoryExtensions;
static BufferedMemoryExtensions s_memoryExtensions;

GLMemoryExtensions* GLMemoryExtensions::Get(unsigned int contextID, bool createIfNotInitalized)
{
    if (!s_memoryExtensions[contextID] && createIfNotInitalized)
        s_memoryExtensions[contextID] = new GLMemoryExtensions(contextID);

    return s_memoryExtensions[contextID].get();
}

GLMemoryExtensions::GLMemoryExtensions(unsigned int in_contextID)
{
    // general
    osg::setGLExtensionFuncPtr(glGetInternalformativ, "glGetInternalformativ");

    osg::setGLExtensionFuncPtr(glCreateTextures, "glCreateTextures");
    osg::setGLExtensionFuncPtr(glTextureParameteri, "glTextureParameteri");

    osg::setGLExtensionFuncPtr(glCopyImageSubData, "glCopyImageSubData");
    

    // memory objects
    osg::setGLExtensionFuncPtr(glCreateMemoryObjectsEXT, "glCreateMemoryObjectsEXT");
    osg::setGLExtensionFuncPtr(glMemoryObjectParameterivEXT, "glMemoryObjectParameterivEXT");

#if WIN32
    osg::setGLExtensionFuncPtr(glImportMemoryWin32HandleEXT, "glImportMemoryWin32HandleEXT");
#else
    osg::setGLExtensionFuncPtr(glImportMemoryFdEXT, "glImportMemoryFdEXT");
#endif

    osg::setGLExtensionFuncPtr(glTextureStorageMem2DEXT, "glTextureStorageMem2DEXT");

    // semaphores
    osg::setGLExtensionFuncPtr(glGenSemaphoresEXT, "glGenSemaphoresEXT");
    osg::setGLExtensionFuncPtr(glDeleteSemaphoresEXT, "glDeleteSemaphoresEXT");
    osg::setGLExtensionFuncPtr(glIsSemaphoreEXT, "glIsSemaphoreEXT");
    osg::setGLExtensionFuncPtr(glSemaphoreParameterui64vEXT, "glSemaphoreParameterui64vEXT");
    osg::setGLExtensionFuncPtr(glGetSemaphoreParameterui64vEXT, "glGetSemaphoreParameterui64vEXT");
    osg::setGLExtensionFuncPtr(glWaitSemaphoreEXT, "glWaitSemaphoreEXT");
    osg::setGLExtensionFuncPtr(glSignalSemaphoreEXT, "glSignalSemaphoreEXT");

#if WIN32
    osg::setGLExtensionFuncPtr(glImportSemaphoreWin32HandleEXT, "glImportSemaphoreWin32HandleEXT");
#else
    osg::setGLExtensionFuncPtr(glImportSemaphoreFdEXT, "glImportSemaphoreFdEXT");
#endif
}
