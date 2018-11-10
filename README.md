osg2vsg is a small utility library that converts OpenSceneGraph images and 3D modules into [VSG](https://github.com/vsg-dev/VulkanSceneGraphPrototype)/Vulkan equivalents.


## include/osg2vsg/ImageUtils.h functionality
Currently only loading and conversion of images are supported, provided by the [include/osg2vsg/ImageUtils.h](include/osg2vsg/ImageUtils.h) header. Three convenience functions are available:

To help map OpenGL image formats to Vulkan equivalents use osg2vsg::convertGLImageFormatToVulkan(GLenum dataType, GLenum pixelFormat) method.  Vulkan only supports a subset of formats that OpenGL formats, so make sure they are compatible i.e. you must use RGBA rather than RGB. Typical usage:

```c++
osg::ref_ptr<osg::Image> image = osgDB::readImageFile("myfile.png");
GLenum dataType = image->getDataType();
GLenum pixelFormat = image->getPixelFormat();
VkFormat format = osg2vsg::convertGLImageFormatToVulkan(dataType, pixelFormat);
```

If the osg::Image is not a compatible format you can use the  osg2vsg::formatImage(osg::Image* image, GLenum pixelFormat) function to convert the image to a new pixel format.  typical usage:

```c++
osg::ref_ptr<osg::Image> image = osgDB::readImageFile("myfile.png");
if (image->getPixelFormat()!=GL_RGBA) image = formatImage(image.get(), GL_RGBA);
```

To provide a convenient way to load images and create a VSG/Vulkan compatible image the readImageFile(..) function, this uses the OpenSceneGraph to load an image, converts to a compatible Vulkan format if the image isn't already compatible, then sets and copies the image data to the Vulkan memory.

```c++
vsg::ImageData imageData = osg2vsg::readImageFile(device, commandPool, graphicsQueue, "myfile.png")
```

An example of osg2vsg::readImageFile(..) being used be be found in the [vsgExamples](https://github.com/vsg-dev/vsgExamples) repository's [vsgdraw](https://github.com/vsg-dev/vsgExamples/tree/master/examples_osg2vsg/vsgdraw).

## Building and installing osg2vsg

To build and install in source, with all dependencies installed in standard system directories:

    git clone https://github.com/vsg-dev/osg2vsg.git
    cd osg2vsg
    cmake .
    make -j 8
    sudo make install
