#include <vsg/all.h>

#include <osg/ArgumentParser>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgUtil/Optimizer>
#include <osgUtil/MeshOptimizers>

#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/SceneBuilder.h>
#include <osg2vsg/Optimize.h>

#include "ConvertToVsg.h"

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);

    auto buildOptions = osg2vsg::BuildOptions::create();
    auto inputFilename = arguments.value(std::string(),"-i");

    vsg::Path outputFilename;
    if (arguments.read("-o", outputFilename)) buildOptions->extension = vsg::fileExtension(outputFilename);

    auto levels = arguments.value(30, "-l");
    uint32_t numThreads = arguments.value(16, "-t");
    uint32_t numTilesBelow = arguments.value(0, "-n");

    if (arguments.read("--ext", buildOptions->extension)) {}
    if (arguments.read("--cull-nodes")) buildOptions->insertCullNodes = true;
    if (arguments.read("--no-cull-nodes")) buildOptions->insertCullNodes = false;
    if (arguments.read("--no-culling")) { buildOptions->insertCullGroups = false; buildOptions->insertCullNodes = false; }
    if (arguments.read("--billboard-transform")) { buildOptions->billboardTransform = true; }
    if (arguments.read("--Geometry")) { buildOptions->geometryTarget = osg2vsg::VSG_GEOMETRY; }
    if (arguments.read("--VertexIndexDraw")) { buildOptions->geometryTarget = osg2vsg::VSG_VERTEXINDEXDRAW; }
    if (arguments.read("--Commands")) { buildOptions->geometryTarget = osg2vsg::VSG_COMMANDS; }
    if (arguments.read({"--bind-single-ds", "--bsds"})) buildOptions->useBindDescriptorSet = true;

    if (inputFilename.empty() || outputFilename.empty())
    {
        std::cout<<"Please support an input and output filenames via -i inputfilename.ext -o outputfile.ext"<<std::endl;
        return 1;
    }

    auto status = vsg::ActivityStatus::create();
    auto operationThreads = vsg::OperationThreads::create(numThreads, status);
    auto operationQueue = operationThreads->queue;
    auto latch = vsg::Latch::create(1); // 1 for the ReadOperation we're about to add

    struct ReadOperation : public vsg::Operation
    {
        ReadOperation(vsg::observer_ptr<vsg::OperationQueue> q, vsg::ref_ptr<vsg::Latch> l, vsg::ref_ptr<const osg2vsg::BuildOptions> bo,
                      const std::string& inPath, const std::string& inFilename,
                      const vsg::Path& outPath, const vsg::Path& outFilename,
                      int cl, int ml, uint32_t ntb,
                      vsg::ref_ptr<vsg::StateGroup> in_inheritedStateGroup = {}) :
            queue(q),
            latch(l),
            buildOptions(bo),
            inputPath(inPath),
            inputFilename(inFilename),
            outputPath(outPath),
            outputFilename(outFilename),
            level(cl),
            maxLevel(ml),
            numTilesBelow(ntb),
            inheritedStateGroup(in_inheritedStateGroup)
        {
        }

        vsg::ref_ptr<vsg::StateGroup> decorateWithStateGroupIfAppropriate(vsg::ref_ptr<vsg::Node> scene)
        {
            struct FindStateGroup : public vsg::Visitor
            {
                std::map<vsg::ref_ptr<vsg::BindGraphicsPipeline>, std::list<vsg::StateGroup*>> pipleStateGroupMap;

                void apply(vsg::Node& node) override
                {
                    node.traverse(*this);
                }

                void apply(vsg::StateGroup& stateGroup) override
                {
                    for(auto& statecommand : stateGroup.getStateCommands())
                    {
                        auto bgp = statecommand.cast<vsg::BindGraphicsPipeline>();
                        if (bgp) pipleStateGroupMap[bgp].push_back(&stateGroup);
                    }
                }
            } findStateGroup;

            scene->accept(findStateGroup);

            if (findStateGroup.pipleStateGroupMap.size()==1)
            {
                auto root = vsg::StateGroup::create();
                auto itr = findStateGroup.pipleStateGroupMap.begin();

                // add BindGraphicsPipeline to new root StateGroup
                root->add(itr->first);

                // remove from original StateGroup
                for(auto& sg : itr->second)
                {
                    sg->remove(itr->first);
                }

                root->addChild(scene);

                // if an EllipsoidModel is assigned to the scene move to the new root.
                auto em = scene->getObject("EllipsoidModel");
                if (em)
                {
                    root->setObject("EllipsoidModel", em);
                    scene->removeObject("EllipsoidModel");
                }

                return root;
            }
            else
            {
                return {};
            }
        }

        void run() override
        {
            std::string combinedInputFilename = osgDB::concatPaths(inputPath, inputFilename);
            std::string combinedOutputFilename = vsg::concatPaths(outputPath, outputFilename);

            std::string finalInputPath = osgDB::getFilePath(combinedInputFilename);
            std::string finalOutputPath = vsg::filePath(combinedOutputFilename);

            osg::ref_ptr<osg::Node> osg_scene = osgDB::readRefNodeFile(combinedInputFilename);

            if (osg_scene)
            {
                osg2vsg::ConvertToVsg sceneBuilder(buildOptions, level, maxLevel, numTilesBelow, inheritedStateGroup);

                sceneBuilder.optimize(osg_scene);

                auto vsg_scene = sceneBuilder.convert(osg_scene);

                if (vsg_scene)
                {
                    if (level==0 && !inheritedStateGroup)
                    {
                        inheritedStateGroup = dynamic_cast<vsg::StateGroup*>(vsg_scene.get());
                        if (!inheritedStateGroup)
                        {
                            inheritedStateGroup = decorateWithStateGroupIfAppropriate(vsg_scene);

                            // if we have inserted a StateGroup to the top, now use this as the scene to write out
                            if (inheritedStateGroup) vsg_scene = inheritedStateGroup;
                        }
                    }

                    if (!finalOutputPath.empty() && !vsg::fileExists(finalOutputPath))
                    {
                        osgDB::makeDirectory(finalOutputPath);
                    }

                    if (level==0)
                    {
                        uint32_t maxNumOfTilesBelow = 0;
                        for(int i=level; i<maxLevel; ++i)
                        {
                            maxNumOfTilesBelow += std::pow(4, i-level);
                        }

                        uint32_t tileMultiplier = std::min(maxNumOfTilesBelow, numTilesBelow) + 1;

                        vsg::CollectDescriptorStats collectStats;
                        vsg_scene->accept(collectStats);

                        auto resourceHints = vsg::ResourceHints::create();

                        resourceHints->maxSlot = collectStats.maxSlot;
                        resourceHints->numDescriptorSets = collectStats.computeNumDescriptorSets() * tileMultiplier;
                        resourceHints->descriptorPoolSizes = collectStats.computeDescriptorPoolSizes();
                        for(auto& poolSize : resourceHints->descriptorPoolSizes)
                        {
                            poolSize.descriptorCount = poolSize.descriptorCount * tileMultiplier;
                        }

                        vsg_scene->setObject("ResourceHints", resourceHints);
                    }

                    vsg::write(vsg_scene, combinedOutputFilename);
                }

                vsg::ref_ptr<vsg::OperationQueue> ref_queue = queue;

                if (ref_queue && level<maxLevel)
                {
                    for(auto& [osg_filename, vsg_filename] : sceneBuilder.filenameMap)
                    {
                        // increment the latch as we are adding another operation to do.
                        latch->count_up();

                        ref_queue->add(vsg::ref_ptr<ReadOperation>(new ReadOperation(queue, latch, buildOptions, finalInputPath, osg_filename, finalOutputPath, vsg_filename, level+1, maxLevel, numTilesBelow, inheritedStateGroup)));
                    }
                }
            }
            // we have finsihed this read operation so decrement the latch, which will release and threads waiting on it.
            latch->count_down();

            // report the number of read/write operations that are pending
            {
                static std::mutex s_io_mutex;
                std::lock_guard<std::mutex> guard(s_io_mutex);
                std::cout<<" "<<latch->count();
                std::cout.flush();
            }
        }

        vsg::observer_ptr<vsg::OperationQueue> queue;
        vsg::ref_ptr<vsg::Latch> latch;
        vsg::ref_ptr<const osg2vsg::BuildOptions> buildOptions;
        std::string inputPath;
        std::string inputFilename;
        vsg::Path outputPath;
        vsg::Path outputFilename;
        int level;
        int maxLevel;
        uint32_t numTilesBelow;
        vsg::ref_ptr<vsg::StateGroup> inheritedStateGroup;
    };

    std::cout<<"read/write operations pending = ";

    vsg::observer_ptr<vsg::OperationQueue> obs_queue(operationQueue);

    operationQueue->add(vsg::ref_ptr<ReadOperation>(new ReadOperation(obs_queue, latch, buildOptions, "", inputFilename, "", outputFilename, 0, levels, numTilesBelow)));

    // wait until the latch goes zero i.e. all read operations have completed
    latch->wait();

    // signal that we are finished and the thread should close
    status->set(false);

    return 1;
}
