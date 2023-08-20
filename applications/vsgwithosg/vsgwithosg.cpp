#include <vsg/all.h>

#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/StateSetManipulator>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osg2vsg/OSG.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

vsg::ref_ptr<vsg::Node> createTextureQuad(vsg::ref_ptr<vsg::Data> sourceData)
{
    struct ConvertToRGBA : public vsg::Visitor
    {
        vsg::ref_ptr<vsg::Data> textureData;

        void apply(vsg::Data& data) override
        {
            textureData = &data;
        }

        void apply(vsg::uintArray2D& fa) override
        {
            // treat as a 24bit depth buffer
            float div = 1.0f / static_cast<float>(1<<24);

            auto rgba = vsg::vec4Array2D::create(fa.width(), fa.height(), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
            auto dest_itr = rgba->begin();
            for(auto& v : fa)
            {
                float m = static_cast<float>(v) * div;
                (*dest_itr++).set(m, m, m, 1.0);
            }
            textureData = rgba;
        }

        void apply(vsg::floatArray2D& fa) override
        {
            auto rgba = vsg::vec4Array2D::create(fa.width(), fa.height(), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
            auto dest_itr = rgba->begin();
            for(auto& v : fa)
            {
                (*dest_itr++).set(v, v, v, 1.0);
            }
            textureData = rgba;
        }

        vsg::ref_ptr<vsg::Data> convert(vsg::ref_ptr<vsg::Data> data)
        {
            data->accept(*this);
            return textureData;
        }


    } convertToRGBA;

    auto textureData = convertToRGBA.convert(sourceData);
    if (!textureData) return {};


    // set up search paths to SPIRV shaders and textures
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // load shaders
    vsg::ref_ptr<vsg::ShaderStage> vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
    vsg::ref_ptr<vsg::ShaderStage> fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
    if (!vertexShader || !fragmentShader)
    {
        std::cout<<"Could not create shaders."<<std::endl;
        return {};
    }

    // set up graphics pipeline
    vsg::DescriptorSetLayoutBindings descriptorBindings
    {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorType, descriptorCount, stageFlags, pImmutableSamplers }
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
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
        vsg::VertexInputState::create( vertexBindingsDescriptions, vertexAttributeDescriptions ),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(),
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // create texture image and associated DescriptorSets and binding
    auto texture = vsg::DescriptorImage::create(vsg::Sampler::create(), textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{texture});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, vsg::DescriptorSets{descriptorSet});

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
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
        {0.0f, 0.0f, 0.0f},
        {static_cast<float>(textureData->width()), 0.0f, 0.0f},
        {static_cast<float>(textureData->width()), 0.0f, static_cast<float>(textureData->height())},
        {0.0f, 0.0f, static_cast<float>(textureData->height())}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_INSTANCE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto colors = vsg::vec3Array::create(
    {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    uint8_t origin = textureData->properties.origin; // in Vulkan the origin is in the top left corner by default.
    float left = 0.0f;
    float right = 1.0f;
    float top = (origin == vsg::TOP_LEFT) ? 0.0f : 1.0f;
    float bottom = (origin == vsg::TOP_LEFT) ? 1.0f : 0.0f;
    auto texcoords = vsg::vec2Array::create(
    {
        {left, bottom},
        {right, bottom},
        {right, top},
        {left, top}
    }); // VK_FORMAT_R32G32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    auto indices = vsg::ushortArray::create(
    {
        0, 1, 2,
        2, 3, 0
    }); // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE

    // setup geometry
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, colors, texcoords}));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    // add drawCommands to transform
    transform->addChild(drawCommands);

    return scenegraph;
}
int main(int argc, char** argv)
{
    // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // add OpenSceneGraph's support for reading and writing 3rd party file formats
    options->add(vsg::VSG::create());
    options->add(vsg::spirv::create());
    options->add(osg2vsg::OSG::create());

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "VulkanSceneGraph Window";
    windowTraits->width = 800;
    windowTraits->height = 600;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    arguments.read(options);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    arguments.read("--screen", windowTraits->screenNum);
    arguments.read("--display", windowTraits->display);
    auto pathFilename = arguments.value<vsg::Path>("","-p");
    auto horizonMountainHeight = arguments.value(0.0, "--hmh");

    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc <= 1)
    {
        std::cout<<"Please specify a model to load on command line."<<std::endl;
    }

    vsg::Path filename = arguments[1];

    // load VulkanSceneGraph scene graph
    auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);

    if (!vsg_scene)
    {
        std::cout<<"vsg::read() could not read : "<<filename<<std::endl;
        return 1;
    }

    // load OpenSceneGraph scene graph
    auto osg_scene = osgDB::readRefNodeFile(filename.string());
    if (!osg_scene)
    {
        std::cout<<"osgDB::readRefNodeFile() could not read : "<<filename<<std::endl;
        return 1;
    }


    // create the VulkanSceneGraph viewer and assign window(s) to it
    auto vsg_viewer = vsg::Viewer::create();
    {
        vsg::ref_ptr<vsg::Window> vsg_window(vsg::Window::create(windowTraits));
        if (!vsg_window)
        {
            std::cout<<"Could not create window."<<std::endl;
            return 1;
        }

        vsg_viewer->addWindow(vsg_window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
        double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
        double nearFarRatio = 0.001;

        // set up the camera
        auto lookAt = vsg::LookAt::create(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
        vsg::ref_ptr<vsg::ProjectionMatrix> perspective;
        if (vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel(vsg_scene->getObject<vsg::EllipsoidModel>("EllipsoidModel")); ellipsoidModel)
        {
            perspective = vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, 30.0, static_cast<double>(vsg_window->extent2D().width) / static_cast<double>(vsg_window->extent2D().height), nearFarRatio, horizonMountainHeight);
        }
        else
        {
            perspective = vsg::Perspective::create(30.0, static_cast<double>(vsg_window->extent2D().width) / static_cast<double>(vsg_window->extent2D().height), nearFarRatio*radius, radius * 4.5);
        }
        auto vsg_camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(vsg_window->extent2D()));

        // add close handler to respond to the close window button and pressing escape
        vsg_viewer->addEventHandler(vsg::CloseHandler::create(vsg_viewer));

        if (pathFilename)
        {
            auto animationPath = vsg::read_cast<vsg::AnimationPath>(pathFilename, options);
            if (!animationPath)
            {
                std::cout<<"Warning: unable to read animation path : "<<pathFilename<<std::endl;
                return 1;
            }

            auto animationPathHandler = vsg::AnimationPathHandler::create(vsg_camera, animationPath, vsg_viewer->start_point());
            animationPathHandler->printFrameStatsToConsole = true;
            vsg_viewer->addEventHandler(animationPathHandler);
        }
        else vsg_viewer->addEventHandler(vsg::Trackball::create(vsg_camera));

        auto commandGraph = vsg::createCommandGraphForView(vsg_window, vsg_camera, vsg_scene);
        vsg_viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        vsg_viewer->compile();
    }

    // set up OpenSceneGraph viewer
    osgViewer::Viewer osg_viewer;
    {
        osg_viewer.setSceneData(osg_scene);

        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        traits->x = windowTraits->width;
        traits->y = 0;
        traits->width = windowTraits->width;
        traits->height = windowTraits->height;
        traits->windowDecoration = true;
        traits->doubleBuffer = true;
        traits->pbuffer = false;

        osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());
        if (!gc || !gc->valid())
        {
            osg::notify(osg::NOTICE)<<"Error: unable to create graphics window"<<std::endl;
            return 1;
        }

        osg_viewer.getCamera()->setGraphicsContext(gc);
        osg_viewer.getCamera()->setViewport(new osg::Viewport(0, 0, windowTraits->width, windowTraits->height));

        osg_viewer.addEventHandler(new osgGA::StateSetManipulator(osg_viewer.getCamera()->getOrCreateStateSet()));
        osg_viewer.addEventHandler(new osgViewer::StatsHandler);

        if (pathFilename.empty())  osg_viewer.setCameraManipulator(new osgGA::TrackballManipulator());
        else osg_viewer.setCameraManipulator(new osgGA::AnimationPathManipulator(pathFilename.string()));

        osg_viewer.realize();

        auto osg_window = dynamic_cast<osgViewer::GraphicsWindow*>(osg_viewer.getCamera()->getGraphicsContext());
        if (osg_window) osg_window->setWindowName("OpenSceneGraph Window");
    }

    // rendering main loop
    while (vsg_viewer->advanceToNextFrame() && !osg_viewer.done())
    {
        // render VulkanSceneGraph frame
        {
            vsg_viewer->handleEvents();
            vsg_viewer->update();
            vsg_viewer->recordAndSubmit();
            vsg_viewer->present();
        }

        // render OpenSceneGraph frame
        {
            osg_viewer.advance();
            osg_viewer.updateTraversal();
            osg_viewer.eventTraversal();
            osg_viewer.renderingTraversals();
        }
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
