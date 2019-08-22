
// code based off glinterop example by jherico and khronos group
// https://github.com/jherico/Vulkan/blob/gl_test2/examples/glinterop/glinterop.cpp
// https://github.com/KhronosGroup/VK-GL-CTS/blob/master/external/vulkancts/modules/vulkan/synchronization/vktSynchronizationCrossInstanceSharingTests.cpp

#define NOMINMAX

// should this be moved into vsg instance header??
#if defined(WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(APPLE)
#define VK_USE_PLATFORM_MACOS_MVK
#elif defined(UNIX)
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <iostream>
#include <vector>
#include <set>

#include <vulkan/vulkan.h>
#include <vsg/all.h>


#include <gl/GL.h>
#include <osg/GL>
#include <osg/GLExtensions>
#include <osg/Texture>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osgViewer/Viewer>


#if defined(WIN32)
using HandleType = HANDLE;
const auto semaphoreHandleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
const auto memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
using HandleType = int;
const auto semaphoreHandleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
const auto memoryHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif


#define GL_TEXTURE_TILING_EXT             0x9580
#define GL_DEDICATED_MEMORY_OBJECT_EXT    0x9581
#define GL_PROTECTED_MEMORY_OBJECT_EXT    0x959B
#define GL_NUM_TILING_TYPES_EXT           0x9582
#define GL_TILING_TYPES_EXT               0x9583
#define GL_OPTIMAL_TILING_EXT             0x9584
#define GL_LINEAR_TILING_EXT              0x9585

#define GL_LAYOUT_GENERAL_EXT             0x958D

#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT   0x9587


class GLMemoryObjectExtensions : public osg::Referenced
{
public:
    void (GL_APIENTRY * glGetInternalformativ) (GLenum, GLenum, GLenum, GLsizei, GLint*);

    void (GL_APIENTRY * glCreateTextures) (GLenum, GLsizei, GLuint*);
    void (GL_APIENTRY * glTextureParameteri) (GLenum, GLenum, GLint);
    
    void (GL_APIENTRY * glCreateMemoryObjectsEXT) (GLsizei, GLuint*);
    void (GL_APIENTRY * glMemoryObjectParameterivEXT) (GLuint, GLenum, const GLint*);

    void (GL_APIENTRY * glTextureStorageMem2DEXT) (GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLuint, GLuint64);

#if WIN32
    void (GL_APIENTRY * glImportMemoryWin32HandleEXT) (GLuint, GLuint64, GLenum, void*);
#else
    void (GL_APIENTRY * glImportMemoryFdEXT) (GLuint, GLuint64, GLenum, GLint);
#endif

    GLMemoryObjectExtensions(unsigned int in_contextID)
    {
        osg::setGLExtensionFuncPtr(glGetInternalformativ, "glGetInternalformativ");
        
        osg::setGLExtensionFuncPtr(glCreateTextures, "glCreateTextures");
        osg::setGLExtensionFuncPtr(glTextureParameteri, "glTextureParameteri");

        osg::setGLExtensionFuncPtr(glCreateMemoryObjectsEXT, "glCreateMemoryObjectsEXT");
        osg::setGLExtensionFuncPtr(glMemoryObjectParameterivEXT, "glMemoryObjectParameterivEXT");

        osg::setGLExtensionFuncPtr(glTextureStorageMem2DEXT, "glTextureStorageMem2DEXT");

#if WIN32
        osg::setGLExtensionFuncPtr(glImportMemoryWin32HandleEXT, "glImportMemoryWin32HandleEXT");
#else
        osg::setGLExtensionFuncPtr(glImportMemoryFdEXT, "glImportMemoryFdEXT");
#endif
    }

};

osg::ref_ptr<GLMemoryObjectExtensions> _memExt;


class SharedTexture2D : public osg::Texture2D
{
public:
    SharedTexture2D(HandleType handle, VkImageTiling tiling, bool isDedicated, osg::Image* image) :
        osg::Texture2D(image),
        _handle(handle),
        _tiling(tiling),
        //_memory(0),
        _isDedicated(isDedicated)
    {
        setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
        setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        setUseHardwareMipMapGeneration(false);
        setResizeNonPowerOfTwoHint(false);
    }

    void apply(osg::State& state) const
    {
        const unsigned int contextID = state.getContextID();

        // get the texture object for the current contextID.
        TextureObject* textureObject = getTextureObject(contextID);

        if(textureObject)
        {
            textureObject->bind(state);
        }
        else if(_image.valid() && _image->data())
        {
            osg::GLExtensions * extensions = state.get<osg::GLExtensions>();

            // keep the image around at least till we go out of scope.
            osg::ref_ptr<osg::Image> image = _image;

            // compute the internal texture format, this set the _internalFormat to an appropriate value.
            computeInternalFormat();

            // compute the dimensions of the texture.
            computeRequiredTextureDimensions(state, *image, _textureWidth, _textureHeight, _numMipmapLevels);

            GLenum texStorageSizedInternalFormat = extensions->isTextureStorageEnabled && (_borderWidth == 0) ? selectSizedInternalFormat(_image.get()) : 0;

            textureObject = generateAndAssignTextureObject(contextID, GL_TEXTURE_2D, _numMipmapLevels,
                texStorageSizedInternalFormat != 0 ? texStorageSizedInternalFormat : _internalFormat,
                _textureWidth, _textureHeight, 1, _borderWidth);

            textureObject->bind(state);

            applyTexParameters(GL_TEXTURE_2D, state);

            // update the modified tag to show that it is up to date.
            getModifiedCount(contextID) = image->getModifiedCount();

            if (textureObject->isAllocated() && image->supportsTextureSubloading())
            {

            }
            else
            {
                
                // create memory object
                GLuint memory = 0;
                _memExt->glCreateMemoryObjectsEXT(1, &memory);
                if (_isDedicated)
                {
                    static const GLint DEDICATED_FLAG = GL_TRUE;
                    _memExt->glMemoryObjectParameterivEXT(memory, GL_DEDICATED_MEMORY_OBJECT_EXT, &DEDICATED_FLAG);
                }

#if WIN32
                _memExt->glImportMemoryWin32HandleEXT(memory, _image->getTotalSizeInBytes(), GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle);
#else
                _memExt->glImportMemoryFdEXT(memory, _image->getTotalSizeInBytes(), GL_HANDLE_TYPE_OPAQUE_FD_EXT, _handle);
#endif

                GLuint glTiling = _tiling == VK_IMAGE_TILING_LINEAR ? GL_LINEAR_TILING_EXT : GL_OPTIMAL_TILING_EXT;
                
                glPixelStorei(GL_UNPACK_ALIGNMENT, image->getPacking());

                // create texture storage using the memory
                _memExt->glTextureParameteri(textureObject->id(), GL_TEXTURE_TILING_EXT, glTiling);
                _memExt->glTextureStorageMem2DEXT(textureObject->id(), 1, texStorageSizedInternalFormat, _image->s(), _image->t(), memory, 0);
                
                //extensions->glTexStorage2D(GL_TEXTURE_2D, _numMipmapLevels, texStorageSizedInternalFormat, _textureWidth, _textureWidth);

                unsigned char* dataPtr = (unsigned char*)image->data();
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    _textureWidth, _textureHeight,
                    (GLenum)image->getPixelFormat(),
                    (GLenum)image->getDataType(),
                    dataPtr);

                textureObject->setAllocated(true);
            }
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

public:
    HandleType _handle;
    VkImageTiling _tiling;
    //GLuint _memory;
    bool _isDedicated;
};

std::set<VkImageTiling> getSupportedTiling()
{
    std::set<VkImageTiling> result;

    GLint numTilingTypes = 0;

    void (GL_APIENTRY * glGetInternalformativ) (GLenum, GLenum, GLenum, GLsizei, GLint*);
    osg::setGLExtensionFuncPtr(glGetInternalformativ, "glGetInternalformativ");

    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_TILING_TYPES_EXT, 1, &numTilingTypes);
    // Broken tiling detection on AMD
    if (0 == numTilingTypes)
    {
        result.insert(VK_IMAGE_TILING_LINEAR);
        return result;
    }

    std::vector<GLint> glTilingTypes;
    {
        glTilingTypes.resize(numTilingTypes);
        glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_TILING_TYPES_EXT, numTilingTypes, glTilingTypes.data());
    }

    for (const auto& glTilingType : glTilingTypes)
    {
        switch (glTilingType)
        {
        case GL_LINEAR_TILING_EXT:
            result.insert(VK_IMAGE_TILING_LINEAR);
            break;

        case GL_OPTIMAL_TILING_EXT:
            result.insert(VK_IMAGE_TILING_OPTIMAL);
            break;

        default:
            break;
        }
    }
    return result;
}

namespace vsg
{
    class SharedDescriptorImage : public Inherit<DescriptorImage, SharedDescriptorImage>
    {
    public:
        SharedDescriptorImage(){}

        SharedDescriptorImage(ref_ptr<Sampler> sampler, VkExtent2D dimensions, uint32_t dstBinding = 0, uint32_t dstArrayElement = 0, VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) :
            Inherit(sampler, vsg::ref_ptr<Data>(), dstBinding, dstArrayElement, descriptorType),
            _dimensions(dimensions)
        {
        }

        class MoveToTargetLayoutImageDataCommand : public Command
        {
        public:
            MoveToTargetLayoutImageDataCommand(ImageData imageData) :
                _imageData(imageData) {}

            void dispatch(CommandBuffer& commandBuffer) const override
            {
                ref_ptr<Image> textureImage(_imageData._imageView->getImage());
                VkImageLayout targetImageLayout = _imageData._imageLayout;

                ImageMemoryBarrier postCopyImageMemoryBarrier(
                    0, VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED, targetImageLayout,
                    textureImage);

                postCopyImageMemoryBarrier.cmdPiplineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }

            ImageData _imageData;

        protected:
            virtual ~MoveToTargetLayoutImageDataCommand(){}
        };

        vsg::ImageData createSharedMemoryImageData(Context& context, Sampler* sampler, VkImageLayout targetImageLayout)
        {
            Device* device = context.device;

            // Prefer optimal if available
            auto supportedTiling = getSupportedTiling();
            if (supportedTiling.end() != supportedTiling.find(VK_IMAGE_TILING_OPTIMAL))
            {
                _tiling = VK_IMAGE_TILING_OPTIMAL;
            }

            // query chain
            VkPhysicalDeviceImageFormatInfo2 imageFormatInfo;
            imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
            imageFormatInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageFormatInfo.type = VK_IMAGE_TYPE_2D;
            imageFormatInfo.tiling = _tiling;
            imageFormatInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;// VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

            VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo;
            externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
            externalImageFormatInfo.handleType = memoryHandleType;
            externalImageFormatInfo.pNext = nullptr;

            imageFormatInfo.pNext = &externalImageFormatInfo;

            // results chain
            VkImageFormatProperties2 imageFormatProperties2;
            imageFormatProperties2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

            VkExternalImageFormatProperties externalImageFormatProperties;
            externalImageFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
            externalImageFormatProperties.pNext = nullptr;

            imageFormatProperties2.pNext = &externalImageFormatProperties;
            
            vkGetPhysicalDeviceImageFormatProperties2(device->getPhysicalDevice()->getPhysicalDevice(), &imageFormatInfo, &imageFormatProperties2);

            VkImageFormatProperties imageFormatProperties = imageFormatProperties2.imageFormatProperties;

            if (externalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
            {
                _dedicated = true;
            }
            
            VkImageCreateInfo imageCreateInfo = {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.extent.width = _dimensions.width;
            imageCreateInfo.extent.height = _dimensions.height;
            imageCreateInfo.extent.depth = 1;
            imageCreateInfo.mipLevels = 1;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageCreateInfo.tiling = _tiling;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.usage = imageFormatInfo.usage;//VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.pNext = nullptr;
                
            ref_ptr<Image> textureImage = Image::create(device, imageCreateInfo);
            if (!textureImage)
            {
                return ImageData();
            }            


            // Always add the export info to the memory allocation chain
            VkExportMemoryAllocateInfo exportAllocInfo;
            exportAllocInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
            exportAllocInfo.handleTypes = memoryHandleType;
            exportAllocInfo.pNext = nullptr;

            VkMemoryDedicatedAllocateInfo dedicatedMemAllocInfo;
            if (_dedicated)
            {
                // Potentially add the dedicated memory allocation
                dedicatedMemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
                dedicatedMemAllocInfo.image = textureImage->image();
                dedicatedMemAllocInfo.buffer = VK_NULL_HANDLE;
                dedicatedMemAllocInfo.pNext = nullptr;
                exportAllocInfo.pNext = &dedicatedMemAllocInfo;
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(*device, *textureImage, &memRequirements);

            auto[deviceMemory, offset] = context.deviceMemoryBufferPools.reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &exportAllocInfo);

            if (!deviceMemory)
            {
                std::cout << "Warning: vsg::transferImageData() Failed allocate memory to reserve slot" << std::endl;
                return ImageData();
            }

            textureImage->bind(deviceMemory, offset);

#if WIN32
            VkMemoryGetWin32HandleInfoKHR getHandleInfo;
            getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
#else
            VkMemoryGetFdInfoKHR getHandleInfo;
            getHandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
#endif
            getHandleInfo.handleType = memoryHandleType;
            getHandleInfo.memory = *deviceMemory;
            getHandleInfo.pNext = nullptr;

            auto vkGetMemoryWin32HandleKHR2 = PFN_vkGetMemoryWin32HandleKHR(vkGetDeviceProcAddr(device->getDevice(), "vkGetMemoryWin32HandleKHR"));
            vkGetMemoryWin32HandleKHR2(device->getDevice(), &getHandleInfo, &_memoryHandle);

            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = *textureImage;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = imageCreateInfo.format;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.pNext = nullptr;

            ref_ptr<ImageView> textureImageView = ImageView::create(device, createInfo);
            if (textureImageView) textureImageView->setImage(textureImage);

            ImageData imageData(sampler, textureImageView, targetImageLayout);

            context.commands.emplace_back(new MoveToTargetLayoutImageDataCommand(imageData));

            return imageData;
        }

        void compile(Context& context) override
        {
            // check if we have already compiled the imageData.
            if ((_imageInfos.size() >= _imageDataList.size()) && (_imageInfos.size() >= _samplerImages.size())) return;

            if (!_samplerImages.empty())
            {
                _imageDataList.clear();
                _imageDataList.reserve(_samplerImages.size());
                for (auto& samplerImage : _samplerImages)
                {
                    samplerImage.first->compile(context);
                    _imageDataList.emplace_back(createSharedMemoryImageData(context, samplerImage.first, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
                }
            }

            // convert from VSG to Vk
            _imageInfos.resize(_imageDataList.size());
            for (size_t i = 0; i < _imageDataList.size(); ++i)
            {
                const ImageData& data = _imageDataList[i];
                VkDescriptorImageInfo& info = _imageInfos[i];
                if (data._sampler)
                    info.sampler = *(data._sampler);
                else
                    info.sampler = 0;

                if (data._imageView)
                    info.imageView = *(data._imageView);
                else
                    info.imageView = 0;

                info.imageLayout = data._imageLayout;
            }
        }

    public:
        VkExtent2D _dimensions;
        VkImageTiling _tiling { VK_IMAGE_TILING_LINEAR };
        bool _dedicated {false};

        HandleType _memoryHandle {0};
    };
    VSG_type_name(vsg::SharedDescriptorImage)
}


osg::ref_ptr<SharedTexture2D> _sharedTexture;
vsg::ref_ptr<vsg::SharedDescriptorImage> _sharedImage;

vsg::ref_ptr<vsg::Viewer> createVsgViewer(osg::ref_ptr<osg::Image> image)
{
    auto viewer = vsg::Viewer::create();

    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "vsgGLinterop";

    windowTraits->instanceExtensionNames =
    {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
    };

    windowTraits->deviceExtensionNames =
    {
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#if defined(WIN32)
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#else
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
#endif
    };

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits, true));
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return vsg::ref_ptr<vsg::Viewer>();
    }

    viewer->addWindow(window);

    //
    //

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return vsg::ref_ptr<vsg::Viewer>();;
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    vsg::DescriptorSetLayouts descriptorSetLayouts{ vsg::DescriptorSetLayout::create(descriptorBindings) };

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions
    {
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions
    {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
    };

    vsg::GraphicsPipelineStates pipelineStates
    {
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{ vertexShader, fragmentShader }, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    _sharedImage = vsg::SharedDescriptorImage::create(vsg::Sampler::create(), VkExtent2D {(uint32_t)image->s(), (uint32_t)image->t()} , 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    /*vsg::Path textureFile("textures/lz.vsgb");
    auto textureData = vsg::read_cast<vsg::Data>(vsg::findFile(textureFile, searchPaths));
    auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);*/

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, vsg::Descriptors{ /*texture*/ _sharedImage });
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{ descriptorSet });

    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {
            {-0.5f, -0.5f, 0.0f},
            {0.5f,  -0.5f, 0.05f},
            {0.5f , 0.5f, 0.0f},
            {-0.5f, 0.5f, 0.0f},
            {-0.5f, -0.5f, -0.5f},
            {0.5f,  -0.5f, -0.5f},
            {0.5f , 0.5f, -0.5},
            {-0.5f, 0.5f, -0.5}
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f},
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {
            0, 1, 2,
            2, 3, 0,
            4, 5, 6,
            6, 7, 4
        }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

        // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices, colors, texcoords }));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(12, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    // camera related details
    auto viewport = vsg::ViewportState::create(window->extent2D());
    auto perspective = vsg::Perspective::create(60.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(vsg::dvec3(1.0, 1.0, 1.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(vsg::GraphicsStage::create(scenegraph, camera));

    // compile the Vulkan objects
    viewer->compile();

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({ vsg::CloseHandler::create(viewer) });

    return viewer;
}

void buildOsgDebugScene(osgViewer::Viewer* viewer, osg::Texture2D* texture)
{
    osg::Geometry* quadgeom = osg::createTexturedQuadGeometry(osg::Vec3(0.0f, 0.0f, 0.0f), osg::Vec3(800.0f, 0.0f, 0.0f), osg::Vec3(0.0f, 0.0f, 600.0f));
    quadgeom->setUseVertexBufferObjects(true);
    quadgeom->setUseDisplayList(false);
    osg::Geode* quadgeode = new osg::Geode();
    quadgeode->addDrawable(quadgeom);

    osg::StateSet* stateset = quadgeode->getOrCreateStateSet();

    osg::Program* program = new osg::Program();

    const char* QuadShaderVert =
        "#version 450\n"
        "in vec4 osg_Vertex;\n"
        "in vec4 osg_MultiTexCoord0;\n"
        "out highp vec2 v_texcoord;\n"
        "uniform mat4 osg_ModelViewProjectionMatrix;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
        "    v_texcoord = osg_MultiTexCoord0.xy;\n"
        "}\n";

    const char* QuadShaderFrag =
        "#version 450\n"
        "in highp vec2 v_texcoord;\n"
        "uniform sampler2D texture0;\n"
        "out lowp vec4 fragColor;\n"
        "void main()\n"
        "{\n"
        "    fragColor = texture(texture0, v_texcoord.xy);\n"
        "}\n";

    program->addShader(new osg::Shader(osg::Shader::VERTEX, QuadShaderVert));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, QuadShaderFrag));
    stateset->setAttributeAndModes(program, osg::StateAttribute::ON);

    stateset->setTextureAttribute(0, texture);
    stateset->addUniform(new osg::Uniform("texture0", (unsigned int)0));

    viewer->setSceneData(quadgeode);
}

int main(int /*argc*/, char** /*argv*/)
{
    osg::setNotifyLevel(osg::WARN);
    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile("./image.png");

    osg::ref_ptr< osg::GraphicsContext::Traits > traits = new osg::GraphicsContext::Traits();
    traits->x = 20; traits->y = 20;
    traits->width = 800; traits->height = 600;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->glContextVersion = "4.5";
    osg::ref_ptr< osg::GraphicsContext > gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc.valid())
    {
        osg::notify(osg::FATAL) << "Unable to create OpenGL v" << "4.5" << " context." << std::endl;
        return(1);
    }

    osg::ref_ptr<osgViewer::Viewer> osgviewer = new osgViewer::Viewer();
    osgviewer->setThreadingModel(osgViewer::ViewerBase::SingleThreaded);
    osgviewer->setCameraManipulator(nullptr);
    osgviewer->getCamera()->setViewport(new osg::Viewport(0.0f, 0.0f, 800.0f, 600.0f));
    osgviewer->getCamera()->setViewMatrix(osg::Matrix::identity());
    osgviewer->getCamera()->setProjectionMatrixAsOrtho(0, 800.0f, 0, 600.0f, 0.0, 100);
    osgviewer->getCamera()->setGraphicsContext(gc);
    gc->realize();
    
    gc->makeCurrent();

    if (!_memExt.valid()) _memExt = new GLMemoryObjectExtensions(0);

    vsg::ref_ptr<vsg::Viewer> vsgviewer = createVsgViewer(image);

    _sharedTexture = new SharedTexture2D(_sharedImage->_memoryHandle, _sharedImage->_tiling, _sharedImage->_dedicated, image.get());
    //_sharedTexture = new SharedTexture2D(0, VkImageTiling::VK_IMAGE_TILING_OPTIMAL, false, image.get());
    _sharedTexture->apply(*osgviewer->getCamera()->getGraphicsContext()->getState());

    gc->releaseContext();

    //buildOsgDebugScene(osgviewer, _sharedTexture.get());
    //buildOsgDebugScene(osgviewer, new osg::Texture2D(image.get()));
    //osgviewer->run();

    // main frame loop
    
    while (vsgviewer->advanceToNextFrame())
    {
        // pass any events into EventHandlers assigned to the Viewer
        vsgviewer->handleEvents();

        vsgviewer->populateNextFrame();

        vsgviewer->submitNextFrame();
    }
    
    return 0;
}
