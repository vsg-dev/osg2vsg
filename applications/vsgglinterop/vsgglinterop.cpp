
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

struct OsgScene
{
    osg::ref_ptr<osg::Camera> predrawCamera; // pre vsg
    osg::ref_ptr<osg::Camera> postdrawCamera; // post vsg

    osg::ref_ptr<osg::Texture2D> colorTexture;
    osg::ref_ptr<osg::Texture2D> depthTexture;
};

struct OsgData
{
    osg::ref_ptr<osgViewer::Viewer> viewer;
    osg::ref_ptr<osg::GraphicsContext> gc;
};

VsgScene createVsgScene(vsg::ref_ptr<vsg::Descriptor> image, VkExtent2D viewDimensions, bool useGLPipelineStates)
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
    //depthStencilState->depthCompareOp = useGLPipelineStates ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;

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

VsgData createVsgViewer()
{
    VsgData result;

    auto viewer = vsg::Viewer::create();

    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "vsgGLinterop";
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

void createVsgRttScene(VsgData& vsgdata, vsg::ref_ptr<vsg::ImageView> colorImage, vsg::ref_ptr<vsg::ImageView> depthImage, vsg::ref_ptr<vsg::DescriptorImage> image, bool preview)
{
    // create the rtt scene
    VsgScene rttscene = createVsgScene(image, vsgdata.window->extent2D(), true);

    vsg::ref_ptr<vsg::OffscreenGraphicsStage> offsreenStage = vsg::OffscreenGraphicsStage::create(vsgdata.window->device(), rttscene.scenegraph, rttscene.camera, vsgdata.window->extent2D(), colorImage, depthImage, VK_ATTACHMENT_LOAD_OP_LOAD);
    //vsg::ref_ptr<vsg::OffscreenGraphicsStage> offsreenStage = vsg::OffscreenGraphicsStage::create(vsgdata.window->device(), rttscene.scenegraph, rttscene.camera, vsgdata.window->extent2D());

    // create main scene
    vsg::ref_ptr<vsg::DescriptorImage> rttimage = vsg::DescriptorImage::create(vsg::Sampler::create(), offsreenStage->_colorImageView);
    VsgScene mainscene = createVsgScene(preview ? rttimage : image, vsgdata.window->extent2D(), false);

    vsgdata.window->addStage(offsreenStage);

    // add a GraphicsStage to the Window to do dispatch of the command graph to the commnad buffer(s)
    vsgdata.window->addStage(vsg::GraphicsStage::create(mainscene.scenegraph, mainscene.camera));

    vsgdata.offsreenStage = offsreenStage;
}

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
    colorRendertex->setInternalFormat(GL_RGBA);
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
    // predraw scene
    
    osg::ref_ptr<osg::Geode> premodel = new osg::Geode();
    premodel->addDrawable(new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0.0f, 0.0f, 0.5f), 0.5)));
    premodel->getOrCreateStateSet()->setAttributeAndModes(modelprogram, osg::StateAttribute::ON);
    premodel->getOrCreateStateSet()->addUniform(new osg::Uniform("albedo", osg::Vec4(0.0f,1.0f,0.0f,1.0f)));

    osg::ref_ptr<osg::MatrixTransform> prexform = new osg::MatrixTransform();
    prexform->addChild(premodel.get());

    osg::ref_ptr<osg::Camera> prerttcam = new osg::Camera();
    prerttcam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    prerttcam->setProjectionMatrixAsPerspective(camFov, (float)width / (float)height, camZNear, camZFar);
    prerttcam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    prerttcam->setViewMatrixAsLookAt(osg::Vec3(camPos.x, camPos.y, camPos.z), osg::Vec3(camLookat.x, camLookat.y, camLookat.z), osg::Vec3(camUp.x, camUp.y, camUp.z));
    prerttcam->setViewport(0, 0, width, height);
    prerttcam->setClearColor(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    prerttcam->setRenderOrder(osg::Camera::PRE_RENDER, 0);

    prerttcam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    prerttcam->attach(osg::Camera::COLOR_BUFFER, colorRendertex);
    prerttcam->attach(osg::Camera::DEPTH_BUFFER, depthRendertex);

    prerttcam->addChild(prexform);

    scenegraph->addChild(prerttcam);

    //
    // post draw scene

    osg::ref_ptr<osg::Geode> postmodel = new osg::Geode();
    postmodel->addDrawable(new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0.0f, 0.0f, -0.5f), 0.5)));
    postmodel->getOrCreateStateSet()->setAttributeAndModes(modelprogram, osg::StateAttribute::ON);
    postmodel->getOrCreateStateSet()->addUniform(new osg::Uniform("albedo", osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f)));

    osg::ref_ptr<osg::MatrixTransform> postxform = new osg::MatrixTransform();
    postxform->addChild(postmodel.get());

    osg::ref_ptr<osg::Camera> postrttcam = new osg::Camera();
    postrttcam->setClearMask(0);
    postrttcam->setProjectionMatrixAsPerspective(camFov, (float)width / (float)height, camZNear, camZFar);
    postrttcam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    postrttcam->setViewMatrixAsLookAt(osg::Vec3(camPos.x, camPos.y, camPos.z), osg::Vec3(camLookat.x, camLookat.y, camLookat.z), osg::Vec3(camUp.x, camUp.y, camUp.z));
    postrttcam->setViewport(0, 0, width, height);
    postrttcam->setRenderOrder(osg::Camera::PRE_RENDER, 1);

    postrttcam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    postrttcam->attach(osg::Camera::COLOR_BUFFER, colorRendertex);
    postrttcam->attach(osg::Camera::DEPTH_BUFFER, depthRendertex);

    postrttcam->addChild(postxform);

    scenegraph->addChild(postrttcam);


    // set scene data and return
    result.predrawCamera = prerttcam;
    result.postdrawCamera = postrttcam;
    result.colorTexture = colorRendertex;
    result.depthTexture = depthRendertex;

    osgdata.viewer->setSceneData(scenegraph);

    return result;
}

OsgData createOsgViewer()
{
    OsgData result;

    //osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile("./image.png");

    osg::ref_ptr< osg::GraphicsContext::Traits > traits = new osg::GraphicsContext::Traits();
    traits->x = 20; traits->y = 20;
    traits->width = width; traits->height = height;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    //traits->glContextFlags = WGL_CONTEXT_DEBUG_BIT_ARB;
    traits->glContextVersion = "4.5";
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

/*
void moveToTargetLayout(vsg::Device* device, vsg::CommandPool* commandPool, vsg::VsgImage image, VkAccessFlagBits dstAccessFlags, VkPipelineStageFlagBits dstStageFlags, VkImageAspectFlags aspectFlags, std::vector<VkSemaphore> waits, std::vector<VkPipelineStageFlags> waitStages, std::vector<VkSemaphore> signals)
{
    dispatchCommandsToQueue(device, commandPool, waits, waitStages, signals, device->getQueue(device->getPhysicalDevice()->getGraphicsFamily()), [&](VkCommandBuffer commandBuffer) {
        populateSetLayoutCommands(commandBuffer, image.imageView->getImage(), (VkAccessFlagBits)0, dstAccessFlags, VK_IMAGE_LAYOUT_UNDEFINED, image.targetImageLayout, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStageFlags, aspectFlags);
    });
}

void syncImages(vsg::Device* device, vsg::CommandPool* commandPool, vsg::VsgImage srcImage, vsg::VsgImage dstImage, std::vector<VkSemaphore> waits, std::vector<VkPipelineStageFlags> waitStages, std::vector<VkSemaphore> signals)
{
    dispatchCommandsToQueue(device, commandPool, waits, waitStages, signals, device->getQueue(device->getPhysicalDevice()->getGraphicsFamily()), [&](VkCommandBuffer commandBuffer) {
        // move to transfer layouts
        populateSetLayoutCommands(commandBuffer, srcImage.imageView->getImage(), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, srcImage.targetImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, srcImage.aspectFlags);
        populateSetLayoutCommands(commandBuffer, dstImage.imageView->getImage(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, dstImage.targetImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, dstImage.aspectFlags);
    
        // copy
        VkImageCopy imageCopy{ VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                         {},
                         VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                         {},
                         srcImage.createInfo.extent };
        vkCmdCopyImage(commandBuffer, *srcImage.imageView->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage.imageView->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

        // back to original layouts
        populateSetLayoutCommands(commandBuffer, srcImage.imageView->getImage(), VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcImage.targetImageLayout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, srcImage.aspectFlags);
        populateSetLayoutCommands(commandBuffer, dstImage.imageView->getImage(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstImage.targetImageLayout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, dstImage.aspectFlags);

    });
}
*/

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    // these flags work on windows but produces vulkan debug warnings and depth buffer seems noisy somehow
    VkImageUsageFlags colorUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout colorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageUsageFlags depthUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout depthLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (arguments.read("--CORRECTFLAGS"))
    {
        // these are what I beleive are the correct flags, but cause the shared textures to be empty
        colorUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        depthUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    bool vsgpreview = !arguments.read("--NOPREVIEW");


    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    osg::setNotifyLevel(osg::WARN);

    // create our viewers
    VsgData vsgdata = createVsgViewer();
    OsgData osgdata = createOsgViewer();

    vsg::CompileTraversal compile(vsgdata.device);
    compile.context.commandPool = vsg::CommandPool::create(vsgdata.device, vsgdata.device->getPhysicalDevice()->getGraphicsFamily());
    compile.context.renderPass = vsgdata.window->renderPass();
    compile.context.graphicsQueue = vsgdata.device->getQueue(vsgdata.device->getPhysicalDevice()->getGraphicsFamily());

    // we need our gl context to be current inorder to get the tiling info, ensure our sharetexture is created and create our shared semaphores
    osgdata.gc->realize();
    osgdata.gc->makeCurrent();

    // get tiling mode
    std::set<VkImageTiling> supportedTilingModes = getSupportedTiling(*osgdata.gc->getState(), GL_RGBA8);
    VkImageTiling tilingMode = VK_IMAGE_TILING_LINEAR;
    if (supportedTilingModes.find(VK_IMAGE_TILING_OPTIMAL) != supportedTilingModes.end())
    {
        tilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    supportedTilingModes = getSupportedTiling(*osgdata.gc->getState(), GL_DEPTH24_STENCIL8);
    VkImageTiling depthTilingMode = VK_IMAGE_TILING_LINEAR;
    if (supportedTilingModes.find(VK_IMAGE_TILING_OPTIMAL) != supportedTilingModes.end())
    {
        depthTilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    //
    // create our vsg shared image view

    VkImageCreateInfo sharedColorImageCreateInfo = vsg::createImageCreateInfo(VkExtent2D{ width, height }, VK_FORMAT_R8G8B8A8_UNORM, colorUsageFlags, tilingMode);
    vsg::ImageData sharedColorImageData = vsg::createImageView(compile.context, sharedColorImageCreateInfo, VK_IMAGE_ASPECT_COLOR_BIT, true, colorLayout);
    vsg::SharedImage* sharedColorImage = dynamic_cast<vsg::SharedImage*>(sharedColorImageData._imageView->getImage());

    VkImageCreateInfo sharedDepthImageCreateInfo = vsg::createImageCreateInfo(VkExtent2D{ width, height }, VK_FORMAT_D24_UNORM_S8_UINT, depthUsageFlags, tilingMode);
    vsg::ImageData sharedDepthImageData = vsg::createImageView(compile.context, sharedDepthImageCreateInfo, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, true, depthLayout);
    vsg::SharedImage* sharedDepthImage = dynamic_cast<vsg::SharedImage*>(sharedDepthImageData._imageView->getImage());

    //
    // create our osg shared texture

    osg::ref_ptr<osg::SharedTexture> sharedColorTexture = new osg::SharedTexture(sharedColorImage->_memoryHandle, sharedColorImage->_byteSize, width, height, GL_RGBA8, tilingMode == VK_IMAGE_TILING_OPTIMAL ? GL_OPTIMAL_TILING_EXT : GL_LINEAR_TILING_EXT, sharedColorImage->_dedicated);
    sharedColorTexture->apply(*osgdata.viewer->getCamera()->getGraphicsContext()->getState());

    osg::ref_ptr<osg::SharedTexture> sharedDepthTexture = new osg::SharedTexture(sharedDepthImage->_memoryHandle, sharedDepthImage->_byteSize, width, height, GL_DEPTH24_STENCIL8, depthTilingMode == VK_IMAGE_TILING_OPTIMAL ? GL_OPTIMAL_TILING_EXT : GL_LINEAR_TILING_EXT, sharedDepthImage->_dedicated);
    sharedDepthTexture->apply(*osgdata.viewer->getCamera()->getGraphicsContext()->getState());
    
    //
    // create our vsg and osg shared semaphores

    vsg::ref_ptr<vsg::SharedSemaphore> predrawWaitSemaphore = vsg::createSharedSemaphore(vsgdata.device);
    vsg::ref_ptr<vsg::SharedSemaphore> predrawCompleteSemaphore = vsg::createSharedSemaphore(vsgdata.device);

    vsg::ref_ptr<vsg::SharedSemaphore> postdrawWaitSemaphore = vsg::createSharedSemaphore(vsgdata.device);
    vsg::ref_ptr<vsg::SharedSemaphore> postdrawCompleteSemaphore = vsg::createSharedSemaphore(vsgdata.device);

    osg::ref_ptr<osg::GLSemaphore> glPredrawWaitSemaphore = new osg::GLSemaphore(predrawWaitSemaphore->_handle);
    glPredrawWaitSemaphore->compileGLObjects(*osgdata.gc->getState());

    osg::ref_ptr<osg::GLSemaphore> glPredrawCompleteSemaphore = new osg::GLSemaphore(predrawCompleteSemaphore->_handle);
    glPredrawCompleteSemaphore->compileGLObjects(*osgdata.gc->getState());

    osg::ref_ptr<osg::GLSemaphore> glPostdrawWaitSemaphore = new osg::GLSemaphore(postdrawWaitSemaphore->_handle);
    glPostdrawWaitSemaphore->compileGLObjects(*osgdata.gc->getState());

    osg::ref_ptr<osg::GLSemaphore> glPostdrawCompleteSemaphore = new osg::GLSemaphore(postdrawCompleteSemaphore->_handle);
    glPostdrawCompleteSemaphore->compileGLObjects(*osgdata.gc->getState());

    glFlush();
    // we're done doing direct stuff with the gl context so release
    osgdata.gc->releaseContext();

    //
    // build our scene

    auto vsgscenetexture = vsg::DescriptorImage::create(vsg::Sampler::create(), vsg::read_cast<vsg::Data>(vsg::findFile("textures/lz.vsgb", searchPaths)), 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    createVsgRttScene(vsgdata, sharedColorImageData._imageView, sharedDepthImageData._imageView, vsgscenetexture, vsgpreview);
    //createVsgDisplaySharedTextureScene(vsgdata, vsg::DescriptorImage::create(vsg::Sampler::create(), sharedColorImage.imageView, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER));

    // compile the Vulkan objects
    vsgdata.viewer->compile();

    OsgScene osgscene = createOsgScene(osgdata, nullptr);

    osg::ref_ptr<osg::SyncTextureDrawCallback> syncToSharedCallback = new osg::SyncTextureDrawCallback({ { osgscene.colorTexture, sharedColorTexture }, { osgscene.depthTexture, sharedDepthTexture } }, true, { GL_LAYOUT_COLOR_ATTACHMENT_EXT,  GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, { GL_LAYOUT_COLOR_ATTACHMENT_EXT, GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, glPredrawWaitSemaphore, glPredrawCompleteSemaphore);
    osgscene.predrawCamera->setPostDrawCallback(syncToSharedCallback);

    osg::ref_ptr<osg::SyncTextureDrawCallback> syncFromSharedCallback = new osg::SyncTextureDrawCallback({ { osgscene.colorTexture, sharedColorTexture }, { osgscene.depthTexture, sharedDepthTexture } }, false, { GL_LAYOUT_COLOR_ATTACHMENT_EXT,  GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, { GL_LAYOUT_COLOR_ATTACHMENT_EXT, GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT }, glPostdrawWaitSemaphore, glPredrawWaitSemaphore);// glPostdrawCompleteSemaphore);
    osgscene.postdrawCamera->setPreDrawCallback(syncFromSharedCallback);
    
    // main frame loop
    while (true)
    {
        osgdata.viewer->frame();

        vsgdata.viewer->advanceToNextFrame();

        // pass any events into EventHandlers assigned to the Viewer
        vsgdata.viewer->handleEvents();

        vsgdata.viewer->populateNextFrame();

        vsgdata.viewer->submitNextFrame({ *predrawCompleteSemaphore }, { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT }, { *postdrawWaitSemaphore });// { *predrawWaitSemaphore });
    }
    
    return 0;
}
