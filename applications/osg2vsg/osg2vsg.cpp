#include <vsg/all.h>

#include <iostream>
#include <ostream>
#include <chrono>
#include <thread>

#include <osg/Version>
#include <osg/Billboard>
#include <osg/MatrixTransform>
#include <osg/AnimationPath>
#include <osg/io_utils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osgUtil/MeshOptimizers>

#include <osg2vsg/GraphicsNodes.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/SceneAnalysisVisitor.h>
#include <osg2vsg/ComputeBounds.h>


namespace vsg
{
    class AnimationPathHandler : public Inherit<Visitor, AnimationPathHandler>
    {
    public:
        AnimationPathHandler(ref_ptr<Camera> camera, osg::ref_ptr<osg::AnimationPath> animationPath, clock::time_point start_point) :
            _camera(camera),
            _path(animationPath),
            _start_point(start_point)
        {
            _lookAt = dynamic_cast<LookAt*>(_camera->getViewMatrix());

            if (!_lookAt)
            {
                // TODO: need to work out how to map the original ViewMatrix to a LookAt and back, for now just fallback to assigning our own LookAt
                _lookAt = new LookAt;
            }
        }

        void apply(KeyPressEvent& keyPress) override
        {
            if (keyPress.keyBase==' ')
            {
                _frameCount = 0;
            }
        }

        void apply(FrameEvent& frame) override
        {
            if (_frameCount==0)
            {
                _start_point = frame.frameStamp->time;
            }

            double time = std::chrono::duration<double, std::chrono::seconds::period>(frame.frameStamp->time - _start_point).count();
            if (time > _path->getPeriod())
            {
                double average_framerate = double(_frameCount) / time;
                std::cout<<"Period complete numFrames="<<_frameCount<<", average frame rate = "<<average_framerate<<std::endl;

                // reset time back to start
                _start_point = frame.frameStamp->time;
                time = 0.0;
                _frameCount = 0;
            }

            osg::Matrixd matrix;
            _path->getMatrix(time, matrix);

            vsg::dmat4 vsg_matrix(matrix(0,0), matrix(1,0), matrix(2,0), matrix(3,0),
                                  matrix(0,1), matrix(1,1), matrix(2,1), matrix(3,1),
                                  matrix(0,2), matrix(1,2), matrix(2,2), matrix(3,2),
                                  matrix(0,3), matrix(1,3), matrix(2,3), matrix(3,3));

            _lookAt->set(vsg_matrix);

            ++_frameCount;
        }


    protected:
        ref_ptr<Camera> _camera;
        ref_ptr<LookAt> _lookAt;
        ref_ptr<osg::AnimationPath> _path;
        KeySymbol _homeKey = KEY_Space;
        clock::time_point _start_point;
        unsigned int _frameCount = 0;
    };
}


int main(int argc, char** argv)
{
    // write out command line to aid debugging
    for(int i=0; i<argc; ++i)
    {
        std::cout<<argv[i]<<" ";
    }
    std::cout<<std::endl;

    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "osg2vsg";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    auto numFrames = arguments.value(-1, "-f");
    auto printFrameRate = arguments.read("--fr");
    auto sleepTime = arguments.value(0.0, "--sleep");
    auto writeToFileProgramAndDataSetSets = arguments.read({"--write-stateset", "--ws"});
    auto optimize = !arguments.read("--no-optimize");
    auto coreGenerator = arguments.read({"--core-generator", "--core"});
    auto outputFilename = arguments.value(std::string(), "-o");
    auto pathFilename = arguments.value(std::string(),"-p");
    arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height);
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");


    // read osg models.
    osg::ArgumentParser osg_arguments(&argc, argv);

    osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFiles(osg_arguments);

    if (!osg_scene)
    {
        std::cout<<"No model loaded."<<std::endl;
        return 1;
    }

    if (optimize)
    {
        osgUtil::IndexMeshVisitor imv;
        #if OSG_MIN_VERSION_REQUIRED(3,6,4)
        imv.setGenerateNewIndicesOnAllGeometries(true);
        #endif
        osg_scene->accept(imv);
        imv.makeMesh();

        osgUtil::VertexCacheVisitor vcv;
        osg_scene->accept(vcv);
        vcv.optimizeVertices();

        osgUtil::VertexAccessOrderVisitor vaov;
        osg_scene->accept(vaov);
        vaov.optimizeOrder();

        osgUtil::Optimizer optimizer;
        optimizer.optimize(osg_scene.get(), osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);
    }


    // Collect stats about the loaded scene
    osg2vsg::SceneAnalysisVisitor sceneAnalysis;
    sceneAnalysis.writeToFileProgramAndDataSetSets = writeToFileProgramAndDataSetSets;
    osg_scene->accept(sceneAnalysis);

    // build VSG scene
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (coreGenerator) vsg_scene = sceneAnalysis.createCoreVSG(searchPaths);
    else vsg_scene = sceneAnalysis.createNewVSG(searchPaths);

    if (!outputFilename.empty())
    {
        vsg::Path outputFileExtension = vsg::fileExtension(outputFilename);

        if (osg_scene.valid() && outputFileExtension.compare(0, 3,"osg")==0)
        {
            auto scene = sceneAnalysis.createOSG();
            if (scene.valid())
            {
                osgDB::writeNodeFile(*scene, outputFilename);
            }

            return 1;
        }
        else if (vsg_scene.valid() && outputFileExtension.compare(0, 3,"vsg")==0)
        {
            std::cout<<"Writing file to "<<outputFilename<<std::endl;
            std::ofstream fout(outputFilename);
            vsg::AsciiOutput output(fout);
            output.writeObject("Root", vsg_scene);
            return 1;
        }
    }

    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }

    // create the viewer and assign window(s) to it
    auto viewer = vsg::Viewer::create();

    vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
    if (!window)
    {
        std::cout<<"Could not create windows."<<std::endl;
        return 1;
    }

    viewer->addWindow(window);

    // create high level Vulkan objects associated the main window
    vsg::ref_ptr<vsg::PhysicalDevice> physicalDevice(window->physicalDevice());
    vsg::ref_ptr<vsg::Device> device(window->device());
    vsg::ref_ptr<vsg::Surface> surface(window->surface());
    vsg::ref_ptr<vsg::RenderPass> renderPass(window->renderPass());


    VkQueue graphicsQueue = device->getQueue(physicalDevice->getGraphicsFamily());
    VkQueue presentQueue = device->getQueue(physicalDevice->getPresentFamily());
    if (!graphicsQueue || !presentQueue)
    {
        std::cout<<"Required graphics/present queue not available!"<<std::endl;
        return 1;
    }

    vsg::ref_ptr<vsg::CommandPool> commandPool = vsg::CommandPool::create(device, physicalDevice->getGraphicsFamily());

    // camera related state
    auto viewport = vsg::ViewportState::create(window->extent2D());

    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;

    // set up the camera
    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), 0.1, radius * 4.5));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, viewport));


    // create graphics stage
    auto stage = vsg::GraphicsStage::create(vsg_scene, camera);

    // compile the Vulkan objects
    vsg::CompileTraversal compile;
    compile.context.device = device;
    compile.context.commandPool = commandPool;
    compile.context.renderPass = renderPass;
    compile.context.viewport = viewport;
    compile.context.graphicsQueue = graphicsQueue;
    compile.context.projMatrix = stage->_projMatrix;
    compile.context.viewMatrix = stage->_viewMatrix;

    vsg_scene->accept(compile);

    // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(stage);

    //
    // end of initialize vulkan
    //
    /////////////////////////////////////////////////////////////////////

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    if (pathFilename.empty())
    {
        viewer->addEventHandler(vsg::Trackball::create(camera));
    }
    else
    {
        std::ifstream in(pathFilename);
        if (!in)
        {
            std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
            return 1;
        }

        osg::ref_ptr<osg::AnimationPath> animationPath = new osg::AnimationPath;
        animationPath->setLoopMode(osg::AnimationPath::LOOP);
        animationPath->read(in);

        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
    }

    double time = 0.0;
    while (viewer->active() && (numFrames<0 || (numFrames--)>0))
    {
        // poll events and advance frame counters
        viewer->advance();

        double previousTime = time;
        time = std::chrono::duration<double, std::chrono::seconds::period>(std::chrono::steady_clock::now()-viewer->start_point()).count();
        if (printFrameRate) std::cout<<"time = "<<time<<" fps="<<1.0/(time-previousTime)<<std::endl;

        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (viewer->aquireNextFrame())
        {
            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }

        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleepTime));
    }


    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
