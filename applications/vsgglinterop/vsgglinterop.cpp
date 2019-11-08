
// code based off glinterop example by jherico and khronos group
// https://github.com/jherico/Vulkan/blob/gl_test2/examples/glinterop/glinterop.cpp
// https://github.com/KhronosGroup/VK-GL-CTS/blob/master/external/vulkancts/modules/vulkan/synchronization/vktSynchronizationCrossInstanceSharingTests.cpp

#define NOMINMAX

#include <iostream>
#include <vector>
#include <set>

#if defined(WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#elif defined(ANDROID)
#    define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(APPLE)
#    define VK_USE_PLATFORM_MACOS_MVK
#elif defined(UNIX)
#    define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#include <vsg/all.h>

#include <osg/GL>
#include <osg/GLExtensions>
#include <osg/Image>
#include <osg/Texture>
#include <osg/Texture2D>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>

// osg
#include "SharedTexture.h"
#include "GLSemaphore.h"
#include "GLMemoryExtensions.h"
#include "SyncTextureDrawCallbacks.h"

// vsg
#include "SharedImage.h"
#include "SharedSemaphore.h"
#include "CommandUtils.h"


#define WGL_CONTEXT_DEBUG_BIT_ARB         0x00000001

const uint32_t width = 800;
const uint32_t height = 600;

const vsg::dvec3 camPos = vsg::dvec3(0.5f, -2.0f, 1.0f);
const vsg::dvec3 camLookat = vsg::dvec3(0.0f, 0.0f, 0.0f);
const vsg::dvec3 camUp = vsg::dvec3(0.0f, 0.0f, 1.0f);
const float camFov = 60.0f;
const float camZNear = 0.1f;
const float camZFar = 10.0f;

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

const char* ModelShaderVert =
"#version 450\n"
"in vec4 osg_Vertex;\n"
"out highp vec4 viewVertex;\n"
"in vec3 osg_Normal;\n"
"out highp vec3 viewNormal;\n"
"in vec4 osg_MultiTexCoord0;\n"
"out highp vec2 texcoord;\n"
"uniform mat4 osg_ModelViewProjectionMatrix;\n"
"uniform mat4 osg_ModelViewMatrix;\n"
"uniform mat3 osg_NormalMatrix;\n"
"void main()\n"
"{\n"
"    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
"    viewVertex = osg_ModelViewMatrix * osg_Vertex;\n"
"    viewNormal = normalize(osg_NormalMatrix * osg_Normal);\n"
"    texcoord = osg_MultiTexCoord0.xy;\n"
"}\n";

const char* ModelShaderFrag =
"#version 330\n"
"in highp vec4 viewVertex;\n"
"in highp vec3 viewNormal;\n"
"in highp vec2 texcoord;\n"
"uniform sampler2D texture0;\n"
"uniform vec4 albedo;\n"
"out lowp vec4 fragColor;\n"
"void main()\n"
"{\n"
"    float ndotl = dot(viewNormal, normalize(vec3(0.0,0.0,0.0) - viewVertex.xyz ));\n"
"    fragColor = albedo * max(ndotl, 0.0); //texture(texture0, texcoord.xy);\n"
"}\n";

std::set<VkImageTiling> getSupportedTiling(const osg::State& state, GLenum format)
{
    std::set<VkImageTiling> result;

    auto glTilingTypes = osg::SharedTexture::getSupportedTilingTypesForFormat(state, format);

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

//////////////////////////////////////////////
//
// VSG 
// Viewer and scene creation code
//

struct VsgScene
{
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group> scenegraph;
};

struct VsgData
{
    vsg::ref_ptr<vsg::Viewer> viewer;
    vsg::ref_ptr<vsg::Window> window;
    vsg::ref_ptr<vsg::Device> device;

    vsg::ref_ptr<vsg::OffscreenGraphicsStage> offsreenStage;
};

// create a vsg scene displaying our shared texture

VsgScene createVsgTextureDisplayGraph(vsg::ref_ptr<vsg::Descriptor> image, VkExtent2D viewDimensions, bool useGLPipelineStates)
{
    VsgScene scene;

    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout << "Could not create shaders." << std::endl;
        return scene;
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

    auto depthStencilState = vsg::DepthStencilState::create();

    auto rasterState = vsg::RasterizationState::create();
    rasterState->frontFace = useGLPipelineStates ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;

    vsg::GraphicsPipelineStates pipelineStates
    {
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasterState,
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        depthStencilState
    };

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{ vertexShader, fragmentShader }, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);


    // create texture image and associated DescriptorSets and binding

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, vsg::Descriptors{ image });
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{ descriptorSet });


    // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(bindGraphicsPipeline);
    scenegraph->add(bindDescriptorSets);

    // set up model transformation node
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT

    // add transform to root of the scene graph
    scenegraph->addChild(transform);

    float hquadsize = 1.0f;
    // set up vertex and index arrays
    auto vertices = vsg::vec3Array::create(
        {
            {-hquadsize, -hquadsize, 0.0f},
            {hquadsize, -hquadsize, 0.0f},
            {hquadsize, hquadsize, 0.0f},
            {-hquadsize, hquadsize, 0.0f}
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
        {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto texcoords = vsg::vec2Array::create(
        {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
        {
            0, 1, 2,
            2, 3, 0
        }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{ vertices, colors, texcoords }));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    // camera related details
    auto viewport = vsg::ViewportState::create(viewDimensions);

    if (useGLPipelineStates)
    {
        viewport->getViewport().height = -static_cast<float>(viewDimensions.height);
        viewport->getViewport().y = static_cast<float>(viewDimensions.height);
    }

    auto perspective = vsg::Perspective::create(camFov, static_cast<double>(viewDimensions.width) / static_cast<double>(viewDimensions.height), camZNear, camZFar);
    auto lookAt = vsg::LookAt::create(camPos, camLookat, camUp);
    auto camera = vsg::Camera::create(perspective, lookAt, viewport);

    scene.camera = camera;
    scene.scenegraph = scenegraph;

    return scene;
}

void createVsgTextureDisplayScene(VsgData& vsgdata, vsg::ref_ptr<vsg::Descriptor> image)
{
    VsgScene scene = createVsgTextureDisplayGraph(image, vsgdata.window->extent2D(), false);
    vsg::ref_ptr<vsg::GraphicsStage> graphicsStage = vsg::GraphicsStage::create(scene.scenegraph, scene.camera);
    vsgdata.window->addStage(graphicsStage);
}

// create vsg viewer with required extensions

VsgData createVsgViewer()
{
    VsgData result;

    auto viewer = vsg::Viewer::create();

    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "VSG Window";
    windowTraits->debugLayer = true;
    windowTraits->width = width;
    windowTraits->height = height;

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

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout << "Could not create windows." << std::endl;
        return result;
    }

    viewer->addWindow(window);

    // assign a CloseHandler to the Viewer to respond to pressing Escape or press the window close button
    viewer->addEventHandlers({ vsg::CloseHandler::create(viewer) });

    result.viewer = viewer;
    result.window = window;
    result.device = vsg::ref_ptr<vsg::Device>(window->device());

    return result;
}

////////////////////////////////////////////////////
//
// Osg Code
// Viewer and scene creation code
//

struct OsgScene
{
    osg::ref_ptr<osg::Camera> camera;

    osg::ref_ptr<osg::Texture2D> colorTexture;
    osg::ref_ptr<osg::Texture2D> depthTexture;
};

struct OsgData
{
    osg::ref_ptr<osgViewer::Viewer> viewer;
    osg::ref_ptr<osg::GraphicsContext> gc;
};

// Create osg scene to render object into a texture

OsgScene createOsgScene(OsgData& osgdata, osg::SharedTexture* sharedTexture)
{
    OsgScene result;

    osg::ref_ptr<osg::Group> scenegraph = new osg::Group();

    osg::Camera* orthocam = new osg::Camera();
    orthocam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    orthocam->setRenderOrder(osg::Camera::POST_RENDER);
    orthocam->setClearMask(GL_DEPTH_BUFFER_BIT);
    orthocam->setViewport(0, 0, width, height);
    orthocam->setProjectionMatrixAsOrtho(0, width, 0, height, 0.0, 100);

    scenegraph->addChild(orthocam);

    // quad to display rtt results
    osg::Geometry* quadGeom = osg::createTexturedQuadGeometry(osg::Vec3(0.0f, 0.0f, 0.0f), osg::Vec3(width, 0.0f, 0.0f), osg::Vec3(0.0f, height, 0.0f));
    quadGeom->setUseVertexBufferObjects(true);
    quadGeom->setUseDisplayList(false);
    osg::Geode* quadGeode = new osg::Geode();
    quadGeode->addDrawable(quadGeom);

    orthocam->addChild(quadGeode);

    osg::StateSet* quadStateset = quadGeode->getOrCreateStateSet();

    osg::Program* quadprogram = new osg::Program();
    quadprogram->addShader(new osg::Shader(osg::Shader::VERTEX, QuadShaderVert));
    quadprogram->addShader(new osg::Shader(osg::Shader::FRAGMENT, QuadShaderFrag));
    quadStateset->setAttributeAndModes(quadprogram, osg::StateAttribute::ON);
    quadStateset->addUniform(new osg::Uniform("texture0", (unsigned int)0));

    //
    // create our textures
    osg::ref_ptr<osg::Texture2D> colorRendertex = new osg::Texture2D();
    colorRendertex->setTextureSize(width, height);
    colorRendertex->setInternalFormat(GL_RGBA8); // use sized type to match vsg
    colorRendertex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    colorRendertex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    colorRendertex->setUseHardwareMipMapGeneration(false);
    colorRendertex->setResizeNonPowerOfTwoHint(false);

    osg::ref_ptr<osg::Texture2D> depthRendertex = new osg::Texture2D();
    depthRendertex->setTextureSize(width, height);
    depthRendertex->setInternalFormat(GL_DEPTH24_STENCIL8);
    depthRendertex->setSourceFormat(GL_DEPTH_STENCIL);
    depthRendertex->setSourceType(GL_UNSIGNED_INT_24_8);
    depthRendertex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    depthRendertex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    depthRendertex->setUseHardwareMipMapGeneration(false);
    depthRendertex->setResizeNonPowerOfTwoHint(false);

    // display the color tex on our quad
    quadStateset->setTextureAttribute(0, colorRendertex);


    // program for scene models
    osg::Program* modelprogram = new osg::Program();
    modelprogram->addShader(new osg::Shader(osg::Shader::VERTEX, ModelShaderVert));
    modelprogram->addShader(new osg::Shader(osg::Shader::FRAGMENT, ModelShaderFrag));

    //
    // rtt scene
    
    osg::ref_ptr<osg::Geode> model = new osg::Geode();
    model->addDrawable(new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0.0f, 0.0f, 0.5f), 0.5)));
    model->getOrCreateStateSet()->setAttributeAndModes(modelprogram, osg::StateAttribute::ON);
    model->getOrCreateStateSet()->addUniform(new osg::Uniform("albedo", osg::Vec4(0.0f,1.0f,0.0f,1.0f)));

    osg::ref_ptr<osg::MatrixTransform> xform = new osg::MatrixTransform();
    xform->addChild(model.get());

    osg::ref_ptr<osg::Camera> rttcam = new osg::Camera();
    rttcam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    rttcam->setProjectionMatrixAsPerspective(camFov, (float)width / (float)height, camZNear, camZFar);
    rttcam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    rttcam->setViewMatrixAsLookAt(osg::Vec3(camPos.x, camPos.y, camPos.z), osg::Vec3(camLookat.x, camLookat.y, camLookat.z), osg::Vec3(camUp.x, camUp.y, camUp.z));
    rttcam->setViewport(0, 0, width, height);
    rttcam->setClearColor(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    rttcam->setRenderOrder(osg::Camera::PRE_RENDER, 0);

    rttcam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    rttcam->attach(osg::Camera::COLOR_BUFFER, colorRendertex);
    rttcam->attach(osg::Camera::DEPTH_BUFFER, depthRendertex);

    rttcam->addChild(xform);

    scenegraph->addChild(rttcam);

    // set scene data and return
    result.camera = rttcam;
    result.colorTexture = colorRendertex;
    result.depthTexture = depthRendertex;

    osgdata.viewer->setSceneData(scenegraph);

    return result;
}

// create osg viewer with 4.5 context

OsgData createOsgViewer()
{
    OsgData result;

    osg::ref_ptr< osg::GraphicsContext::Traits > traits = new osg::GraphicsContext::Traits();
    traits->x = width + 20; traits->y = 20;
    traits->width = width; traits->height = height;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    //traits->glContextFlags = WGL_CONTEXT_DEBUG_BIT_ARB;
    traits->glContextVersion = "4.5";
    traits->windowName = "OSG Window";
    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
    if (!gc.valid())
    {
        osg::notify(osg::FATAL) << "Unable to create OpenGL v" << "4.5" << " context." << std::endl;
        return result;
    }

    osg::ref_ptr<osgViewer::Viewer> viewer = new osgViewer::Viewer();
    viewer->setThreadingModel(osgViewer::ViewerBase::SingleThreaded);
    viewer->setCameraManipulator(new osgGA::TrackballManipulator());
    viewer->getCamera()->setViewport(new osg::Viewport(0.0f, 0.0f, width, height));
    viewer->getCamera()->setViewMatrix(osg::Matrix::identity());
    viewer->getCamera()->setProjectionMatrixAsOrtho(0, width, 0, height, 0.0, 100);
    viewer->getCamera()->setGraphicsContext(gc);

    result.viewer = viewer;
    result.gc = gc;

    return result;
}

// Main

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // default tex dimensions to window dimensions
    uint32_t texwidth = width;
    uint32_t texheight = height;

    // usgage and layout should change depending on if the shared texture is used as a standard texture or an rtt attachment, but changing them seems to be causeing issues.
    VkImageUsageFlags colorUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    osg::ref_ptr<osg::Image> osgimage = nullptr;

    bool rttDemo = arguments.read("--osgrtt");
    if (rttDemo)
    {
        //colorUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // these cause validation errors
        //colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    else
    {
        // non rtt load and osg image to use in our shared texture
        osgimage = osgDB::readImageFile(vsg::findFile("textures/osglogo.png", searchPaths));
        if (!osgimage)
        {
            OSG_FATAL << "Failed to load texture extures/osglogo.png, exiting" << std::endl;
            return 0;
        }

        // set tex dimensions to match image so we can load it without resizing
        texwidth = osgimage->s();
        texheight = osgimage->t();
    }

    osg::setNotifyLevel(osg::WARN);

    // create our viewers
    VsgData vsgdata = createVsgViewer();
    OsgData osgdata = createOsgViewer();

    // for convienince create a vsg compile traversal and setup its context to use in the direct createImageView function
    vsg::CompileTraversal compile(vsgdata.device);
    compile.context.commandPool = vsg::CommandPool::create(vsgdata.device, vsgdata.device->getPhysicalDevice()->getGraphicsFamily());
    compile.context.renderPass = vsgdata.window->renderPass();
    compile.context.graphicsQueue = vsgdata.device->getQueue(vsgdata.device->getPhysicalDevice()->getGraphicsFamily());

    // we need our gl context to be current inorder to get the tiling info, create our shared texture and create our shared semaphores
    osgdata.gc->realize();
    osgdata.gc->makeCurrent();

    // get tiling mode
    std::set<VkImageTiling> supportedTilingModes = getSupportedTiling(*osgdata.gc->getState(), GL_RGBA8);
    VkImageTiling tilingMode = VK_IMAGE_TILING_LINEAR;
    if (supportedTilingModes.find(VK_IMAGE_TILING_OPTIMAL) != supportedTilingModes.end())
    {
        tilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    //
    // create our vsg shared image

    VkImageCreateInfo sharedColorImageCreateInfo = vsg::createImageCreateInfo(VkExtent2D{ texwidth, texheight }, VK_FORMAT_R8G8B8A8_UNORM, colorUsageFlags, tilingMode);
    vsg::ImageData sharedColorImageData = vsg::createImageView(compile.context, sharedColorImageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, true, colorLayout);
    vsg::SharedImage* sharedColorImage = dynamic_cast<vsg::SharedImage*>(sharedColorImageData._imageView->getImage());

    //
    // create our osg shared texture using the shared image

    osg::ref_ptr<osg::SharedTexture> sharedColorTexture = new osg::SharedTexture(sharedColorImage->_memoryHandle, sharedColorImage->_byteSize, texwidth, texheight, GL_RGBA8, tilingMode == VK_IMAGE_TILING_OPTIMAL ? GL_OPTIMAL_TILING_EXT : GL_LINEAR_TILING_EXT, sharedColorImage->_dedicated);
    // if it's not an rtt demo just load and osg image into the texute
    if (!rttDemo)
    {
        sharedColorTexture->setImage(osgimage);
    }
    
    sharedColorTexture->apply(*osgdata.viewer->getCamera()->getGraphicsContext()->getState());
    
    //
    // create our vsg and osg shared semaphores

    vsg::ref_ptr<vsg::SharedSemaphore> waitSemaphore = vsg::createSharedSemaphore(vsgdata.device);
    vsg::ref_ptr<vsg::SharedSemaphore> completeSemaphore = vsg::createSharedSemaphore(vsgdata.device);

    osg::ref_ptr<osg::GLSemaphore> glWaitSemaphore = new osg::GLSemaphore(waitSemaphore->_handle);
    glWaitSemaphore->compileGLObjects(*osgdata.gc->getState());

    osg::ref_ptr<osg::GLSemaphore> glCompleteSemaphore = new osg::GLSemaphore(completeSemaphore->_handle);
    glCompleteSemaphore->compileGLObjects(*osgdata.gc->getState());

    glFlush();
    // we're done doing direct stuff with the gl context so release
    osgdata.gc->releaseContext();

    //
    // build our scene vsg scene passing the image to display

    //auto displayTexture = vsg::DescriptorImage::create(vsg::Sampler::create(), vsg::read_cast<vsg::Data>(vsg::findFile("textures/lz.vsgb", searchPaths)), 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto displayTexture = vsg::DescriptorImage::create(vsg::Sampler::create(), sharedColorImageData._imageView, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    createVsgTextureDisplayScene(vsgdata, displayTexture);

    // compile the Vulkan objects
    vsgdata.viewer->compile();

    // if using rtt demo create an osg scene rendering into an rtt and copy it into our shared texture
    OsgScene osgscene;
    if(rttDemo)
    {
        osgscene = createOsgScene(osgdata, nullptr);

        // install a post draw callback to copy the osg rtt texture into our shared texture, waiting on and signaling the semaphores passed in to sync with vsg
        osg::ref_ptr<osg::SyncTextureDrawCallback> syncToSharedCallback = new osg::SyncTextureDrawCallback({ { osgscene.colorTexture, sharedColorTexture } }, true, { GL_LAYOUT_COLOR_ATTACHMENT_EXT,  GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, { GL_LAYOUT_COLOR_ATTACHMENT_EXT, GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, glWaitSemaphore, glCompleteSemaphore);
        osgscene.camera->setPostDrawCallback(syncToSharedCallback);
    }
    
    // main frame loop
    while (true)
    {
        osgdata.viewer->frame();

        vsgdata.viewer->advanceToNextFrame();

        // pass any events into EventHandlers assigned to the Viewer
        vsgdata.viewer->handleEvents();

        vsgdata.viewer->populateNextFrame();

        if(rttDemo)
        {
            // if rtt mode we need to use the semaphores to sync between osg and vsg when copying to the shared texture
            vsgdata.viewer->submitNextFrame({ *completeSemaphore }, { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT }, { *waitSemaphore });
        }
        else
        {
            vsgdata.viewer->submitNextFrame();
        }
    }
    
    return 0;
}
