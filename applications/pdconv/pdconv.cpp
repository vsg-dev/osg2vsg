#include <vsg/all.h>

#include <osg/ArgumentParser>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgUtil/MeshOptimizers>

#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/SceneBuilder.h>
#include <osg2vsg/Optimize.h>

#include "ConvertToVsg.h"

int main(int argc, char** argv)
{
    osg2vsg::ConvertToVsg sceneBuilder;

    vsg::CommandLine arguments(&argc, argv);
    auto levels = arguments.value(3, "-l");
    auto outputFilename = arguments.value(std::string(), "-o");
    if (arguments.read("--cull-nodes")) sceneBuilder.insertCullNodes = true;
    if (arguments.read("--no-cull-nodes")) sceneBuilder.insertCullNodes = false;
    if (arguments.read("--no-culling")) { sceneBuilder.insertCullGroups = false; sceneBuilder.insertCullNodes = false; }
    if (arguments.read("--billboard-transform")) { sceneBuilder.billboardTransform = true; }
    if (arguments.read("--Geometry")) { sceneBuilder.geometryTarget = osg2vsg::VSG_GEOMETRY; }
    if (arguments.read("--VertexIndexDraw")) { sceneBuilder.geometryTarget = osg2vsg::VSG_VERTEXINDEXDRAW; }
    if (arguments.read("--Commands")) { sceneBuilder.geometryTarget = osg2vsg::VSG_COMMANDS; }
    if (arguments.read({"--bind-single-ds", "--bsds"})) sceneBuilder.useBindDescriptorSet = true;

    std::cout<<"levels = "<<levels<<std::endl;
    std::cout<<"outputFilename = "<<outputFilename<<std::endl;

    osg::ArgumentParser osg_arguments(&argc, argv);

    osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFiles(osg_arguments);

    std::cout<<"osg_scene = "<<osg_scene.get()<<std::endl;

    if (osg_scene.valid())
    {
        sceneBuilder.optimize(osg_scene);

        auto vsg_scene = sceneBuilder.convert(osg_scene);

        if (vsg_scene && !outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename);
        }
    }

    return 1;
}
