
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
#include <osg/Texture>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osgViewer/Viewer>

#include "SharedTexture.h"
#include "SharedDescriptorImage.h"

std::set<VkImageTiling> getSupportedTiling(const osg::State& state)
{
    std::set<VkImageTiling> result;

    GLint numTilingTypes = 0;

    const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();

    extensions->glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_TILING_TYPES_EXT, 1, &numTilingTypes);
    // Broken tiling detection on AMD
    if (0 == numTilingTypes)
    {
        result.insert(VK_IMAGE_TILING_LINEAR);
        return result;
    }

    std::vector<GLint> glTilingTypes;
    {
        glTilingTypes.resize(numTilingTypes);
        extensions->glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_TILING_TYPES_EXT, numTilingTypes, glTilingTypes.data());
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

vsg::ref_ptr<vsg::Viewer> createVsgViewer(vsg::ref_ptr<vsg::Descriptor> image)
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

    std::set<VkImageTiling> supportedTilingModes = getSupportedTiling(*gc->getState());

    VkImageTiling tilingMode = VK_IMAGE_TILING_LINEAR;
    if (supportedTilingModes.find(VK_IMAGE_TILING_OPTIMAL) != supportedTilingModes.end())
    {
        tilingMode = VK_IMAGE_TILING_OPTIMAL;
    }

    vsg::ref_ptr<vsg::SharedDescriptorImage> sharedImage = vsg::SharedDescriptorImage::create(vsg::Sampler::create(), VkExtent2D{ (uint32_t)image->s(), (uint32_t)image->t() }, VK_FORMAT_R8G8B8A8_UNORM, tilingMode, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    vsg::ref_ptr<vsg::Viewer> vsgviewer = createVsgViewer(sharedImage);


    osg::ref_ptr<osg::SharedTexture> sharedTexture = new osg::SharedTexture(sharedImage->handle(), sharedImage->byteSize(), sharedImage->dimensions().width, sharedImage->dimensions().height, GL_RGBA8, sharedImage->tiling(), sharedImage->dedicated());
    sharedTexture->setImage(image);
    sharedTexture->apply(*osgviewer->getCamera()->getGraphicsContext()->getState());

    gc->releaseContext();

    //buildOsgDebugScene(osgviewer, _sharedTexture.get());
    //osgviewer->frame();

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
