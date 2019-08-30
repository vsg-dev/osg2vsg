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

    osg2vsg::BuildOptions buildOptions;
    auto inputFilename = arguments.value(std::string(),"-i");

    vsg::Path outputFilename;
    if (arguments.read("-o", outputFilename)) buildOptions.extension = vsg::fileExtension(outputFilename);

    auto levels = arguments.value(20, "-l");
    uint32_t numThreads = arguments.value(16, "-t");

    if (arguments.read("--ext", buildOptions.extension)) {}
    if (arguments.read("--cull-nodes")) buildOptions.insertCullNodes = true;
    if (arguments.read("--no-cull-nodes")) buildOptions.insertCullNodes = false;
    if (arguments.read("--no-culling")) { buildOptions.insertCullGroups = false; buildOptions.insertCullNodes = false; }
    if (arguments.read("--billboard-transform")) { buildOptions.billboardTransform = true; }
    if (arguments.read("--Geometry")) { buildOptions.geometryTarget = osg2vsg::VSG_GEOMETRY; }
    if (arguments.read("--VertexIndexDraw")) { buildOptions.geometryTarget = osg2vsg::VSG_VERTEXINDEXDRAW; }
    if (arguments.read("--Commands")) { buildOptions.geometryTarget = osg2vsg::VSG_COMMANDS; }
    if (arguments.read({"--bind-single-ds", "--bsds"})) buildOptions.useBindDescriptorSet = true;

    if (inputFilename.empty() || outputFilename.empty())
    {
        std::cout<<"Please support an input and output filenames via -o inputfilename.ext -o outputfile.ext"<<std::endl;
        return 1;
    }

    auto active = vsg::Active::create();
    auto operationThreads = vsg::OperationThreads::create(numThreads, active);
    auto operationQueue = operationThreads->queue;
    auto latch = vsg::Latch::create(1); // 1 for the ReadOperation we're about to add

    struct ReadOperation : public vsg::Operation
    {
        ReadOperation(vsg::observer_ptr<vsg::OperationQueue> q, vsg::ref_ptr<vsg::Latch> l, const osg2vsg::BuildOptions& bo,
                      const std::string& inPath, const std::string& inFilename,
                      const vsg::Path& outPath, const vsg::Path& outFilename,
                      int cl, int ml) :
            queue(q),
            latch(l),
            buildOptions(bo),
            inputPath(inPath),
            inputFilename(inFilename),
            outputPath(outPath),
            outputFilename(outFilename),
            level(cl),
            maxLevel(ml)
        {
        }

        void run() override
        {
            std::string combinedInputFilename = osgDB::concatPaths(inputPath, inputFilename);
            std::string combinedOutputFilename = vsg::concatPaths(outputPath, outputFilename);

            std::string finalInputPath = osgDB::getFilePath(combinedInputFilename);
            std::string finalOutputPath = vsg::filePath(combinedOutputFilename);

            osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFile(combinedInputFilename);

            if (osg_scene)
            {
                osg2vsg::ConvertToVsg sceneBuilder(buildOptions);

                sceneBuilder.optimize(osg_scene);

                auto vsg_scene = sceneBuilder.convert(osg_scene);

                if (vsg_scene)
                {
                    if (!finalOutputPath.empty() && !vsg::fileExists(finalOutputPath))
                    {
                        osgDB::makeDirectory(finalOutputPath);
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

                        ref_queue->add(vsg::ref_ptr<ReadOperation>(new ReadOperation(queue, latch, buildOptions, finalInputPath, osg_filename, finalOutputPath, vsg_filename, level+1, maxLevel)));
                    }
                }
            }
            // we have finsihed this read operation so decrement the latch, which will release and threads waiting on it.
            latch->count_down();

            // report the number of read/write operations that are pending
            {
                static std::mutex s_io_mutex;
                std::lock_guard<std::mutex> guard(s_io_mutex);
                std::cout<<" "<<latch->count().load();
                std::cout.flush();
            }
        }

        vsg::observer_ptr<vsg::OperationQueue> queue;
        vsg::ref_ptr<vsg::Latch> latch;
        osg2vsg::BuildOptions buildOptions;
        std::string inputPath;
        std::string inputFilename;
        vsg::Path outputPath;
        vsg::Path outputFilename;
        int level;
        int maxLevel;
    };

    std::cout<<"read/write operations pending = ";

    vsg::observer_ptr<vsg::OperationQueue> obs_queue(operationQueue);

    operationQueue->add(vsg::ref_ptr<ReadOperation>(new ReadOperation(obs_queue, latch, buildOptions, "", inputFilename, "", outputFilename, 0, levels)));

    // wait until the latch goes zero i.e. all read operations have completed
    latch->wait();

    // signal that we are finished and the thread should close
    active->active = false;

    return 1;
}
