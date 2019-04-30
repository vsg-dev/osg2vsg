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

#include <vsg/core/Objects.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/SceneBuilder.h>
#include <osg2vsg/SceneAnalysis.h>


namespace vsg
{
    class AnimationPathHandler : public Inherit<Visitor, AnimationPathHandler>
    {
    public:
        AnimationPathHandler(ref_ptr<Camera> camera, osg::ref_ptr<osg::AnimationPath> animationPath, clock::time_point start_point, double simulationStep=0.0) :
            _camera(camera),
            _path(animationPath),
            _start_point(start_point),
            _simulationTime(0.0),
            _simulationStep(simulationStep)
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
                _simulationTime = 0.0;
            }

            double actual_time = std::chrono::duration<double, std::chrono::seconds::period>(frame.frameStamp->time - _start_point).count();
            double time = _simulationStep>0.0 ? _simulationTime : actual_time;


            if (time > _path->getPeriod())
            {
                double average_framerate = double(_frameCount) / actual_time;
                std::cout<<"Period complete numFrames="<<_frameCount<<", average frame rate = "<<average_framerate<<std::endl;

                // reset time back to start
                _start_point = frame.frameStamp->time;
                _simulationTime = 0.0;
                _frameCount = 0;
                time = 0.0;
            }

            osg::Matrixd matrix;

            _path->getMatrix(time, matrix);

            vsg::dmat4 vsg_matrix(matrix(0,0), matrix(1,0), matrix(2,0), matrix(3,0),
                                  matrix(0,1), matrix(1,1), matrix(2,1), matrix(3,1),
                                  matrix(0,2), matrix(1,2), matrix(2,2), matrix(3,2),
                                  matrix(0,3), matrix(1,3), matrix(2,3), matrix(3,3));

            _lookAt->set(vsg_matrix);

            ++_frameCount;

            _simulationTime += _simulationStep;
        }


    protected:
        ref_ptr<Camera> _camera;
        ref_ptr<LookAt> _lookAt;
        ref_ptr<osg::AnimationPath> _path;
        KeySymbol _homeKey = KEY_Space;
        clock::time_point _start_point;
        unsigned int _frameCount = 0;

        double _simulationTime;
        double _simulationStep;

    };
}

namespace osg2vsg
{
    class PopulateCommandGraphHandler : public vsg::Inherit<vsg::Visitor, PopulateCommandGraphHandler>
    {
    public:

        bool populateCommandGraph = true;

        PopulateCommandGraphHandler() {}

        void apply(vsg::KeyPressEvent& keyPress) override
        {
            if (keyPress.keyBase=='p')
            {
                populateCommandGraph = !populateCommandGraph;
            }
        }
    };


    class LeafDataCollection : public vsg::Visitor
    {
    public:

        vsg::ref_ptr<vsg::Objects> objects;

        LeafDataCollection()
        {
            objects = new vsg::Objects;
        }

        void apply(vsg::Object& object) override
        {
            if (typeid(object)==typeid(vsg::Texture))
            {
                vsg::Texture* texture = static_cast<vsg::Texture*>(&object);
                if (texture->_textureData)
                {
                    objects->addChild(texture->_textureData);
                }
            }

            object.traverse(*this);
        }

        void apply(vsg::Geometry& geometry) override
        {
            for(auto& data : geometry._arrays)
            {
                objects->addChild(data);
            }
            if (geometry._indices)
            {
                objects->addChild(geometry._indices);
            }
        }

        void apply(vsg::StateGroup& stategroup) override
        {
            for(auto& command : stategroup.getStateCommands())
            {
                command->accept(*this);
            }

            stategroup.traverse(*this);
        }
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

    osg2vsg::SceneBuilder sceneBuilder;
    auto windowTraits = vsg::Window::Traits::create();
    windowTraits->windowTitle = "osg2vsg";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"-t", "--test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->fullscreen = true; }
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read("--no-culling")) sceneBuilder.insertCullGroups = false;
    if (arguments.read("--cull-nodes")) sceneBuilder.insertCullNodes = true;
    if (arguments.read("--no-cull-nodes")) sceneBuilder.insertCullNodes = false;
    if (arguments.read({"--bind-single-ds", "--bsds"})) sceneBuilder.useBindDescriptorSet = true;
    auto numFrames = arguments.value(-1, "-f");
    auto writeToFileProgramAndDataSetSets = arguments.read({"--write-stateset", "--ws"});
    auto optimize = !arguments.read("--no-optimize");
    auto outputFilename = arguments.value(std::string(), "-o");
    auto printStats = arguments.read({"-s", "--stats"});
    auto pathFilename = arguments.value(std::string(),"-p");
    auto batchLeafData = arguments.read("--batch");
    auto simulationFrameRate = arguments.value(0.0, "--sim-fps");
    arguments.read({"--support-mask", "--sm"}, sceneBuilder.supportedShaderModeMask);
    arguments.read({"--override-mask", "--om"}, sceneBuilder.overrideShaderModeMask);
    arguments.read({ "--vertex-shader", "--vert" }, sceneBuilder.vertexShaderPath);
    arguments.read({ "--fragment-shader", "--frag" }, sceneBuilder.fragmentShaderPath);


    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgNodes = std::vector<vsg::ref_ptr<vsg::Node>>;
    VsgNodes vsgNodes;

    // read any vsg files
    vsg::vsgReaderWriter io;

    for (int i=1; i<argc; ++i)
    {
        std::string filename = arguments[i];

        auto before_vsg_load = std::chrono::steady_clock::now();

        auto loaded_scene = io.read<vsg::Node>(filename);

        auto vsg_loadTime = std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_vsg_load).count();

        if (loaded_scene)
        {
            std::cout<<"VSG loadTime = "<<vsg_loadTime<<"ms"<<std::endl;

            vsgNodes.push_back(loaded_scene);
            arguments.remove(i, 1);
            --i;
        }
    }

    // read osg models.
    osg::ArgumentParser osg_arguments(&argc, argv);

    auto before_osg_load = std::chrono::steady_clock::now();

    osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFiles(osg_arguments);

    auto osg_loadTime = std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_osg_load).count();

    if (vsgNodes.empty() && !osg_scene)
    {
        std::cout<<"No model loaded."<<std::endl;
        return 1;
    }

    if (osg_scene.valid())
    {
        std::cout<<"OSG loadTime = "<<osg_loadTime<<"ms"<<std::endl;

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

        // Collect stats for reporting.
        if (printStats)
        {
            osg2vsg::OsgSceneAnalysis osgSceneAnalysis;
            osg_scene->accept(osgSceneAnalysis);
            osgSceneAnalysis._sceneStats->print(std::cout);
        }

        // Collect stats about the loaded scene for the purpose of rebuild it
        sceneBuilder.writeToFileProgramAndDataSetSets = writeToFileProgramAndDataSetSets;
        osg_scene->accept(sceneBuilder);

        // build VSG scene
        vsg::ref_ptr<vsg::Node> converted_vsg_scene = sceneBuilder.createVSG(searchPaths);

        if (converted_vsg_scene)
        {
            vsgNodes.push_back(converted_vsg_scene);
        }

    }

    // assign the vsg_scene from the loaded/converted nodes
    vsg::ref_ptr<vsg::Node> vsg_scene;
    if (vsgNodes.size()>1)
    {
        auto vsg_group = vsg::Group::create();
        for(auto& subgraphs : vsgNodes)
        {
            vsg_group->addChild(subgraphs);
        }

        vsg_scene = vsg_group;
    }
    else if (vsgNodes.size()==1)
    {
        vsg_scene = vsgNodes.front();
    }

    if (!outputFilename.empty())
    {
        vsg::Path outputFileExtension = vsg::fileExtension(outputFilename);

        if (osg_scene.valid() && outputFileExtension.compare(0, 3,"osg")==0)
        {
            auto scene = sceneBuilder.createOSG();
            if (scene.valid())
            {
                osgDB::writeNodeFile(*scene, outputFilename);
            }

            return 1;
        }
        else if (vsg_scene.valid())
        {
            if (batchLeafData)
            {
                osg2vsg::LeafDataCollection leafDataCollection;
                vsg_scene->accept(leafDataCollection);
                vsg_scene->setObject("batch", leafDataCollection.objects);
            }

            if (io.writeFile(vsg_scene, outputFilename))
            {
                return 1;
            }
        }
    }

    if (!vsg_scene)
    {
        std::cout<<"No command graph created."<<std::endl;
        return 1;
    }

    if (printStats)
    {
        osg2vsg::VsgSceneAnalysis vsgSceneAnalysis;
        vsg_scene->accept(vsgSceneAnalysis);
        vsgSceneAnalysis._sceneStats->print(std::cout);
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


    // compute the bounds of the scene graph to help position camera
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
    double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
    double nearFarRatio = 0.0001;

    // set up the camera
    vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 4.5));
    vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0)));
    vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, vsg::ViewportState::create(window->extent2D())));


    // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
    window->addStage(vsg::GraphicsStage::create(vsg_scene, camera));

    auto before_compile = std::chrono::steady_clock::now();

    // compile the Vulkan objects
    viewer->compile();

    std::cout<<"Compile traversal time "<<std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_compile).count()<<"ms"<<std::endl;;

    // add close handler to respond the close window button and pressing esape
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));

    auto populateCommandGraphHandler = osg2vsg::PopulateCommandGraphHandler::create();
    viewer->addEventHandler(populateCommandGraphHandler);

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

        double simulationStep = simulationFrameRate>0.0 ? 1.0/simulationFrameRate : 0.0;
        viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point(), simulationStep));
    }

    // rendering main loop
    while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
    {
        // pass any events into EventHandlers assigned to the Viewer
        viewer->handleEvents();

        if (populateCommandGraphHandler->populateCommandGraph) viewer->populateNextFrame();

        viewer->submitNextFrame();
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
