/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <osg2vsg/OSG.h>
#include <osg2vsg/convert.h>

#include <osg/AnimationPath>
#include <osg/TransferFunction>
#include <osg/io_utils>
#include <osgDB/PluginQuery>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/MeshOptimizers>
#include <osgUtil/Optimizer>

#include "ConvertToVsg.h"
#include "ImageUtils.h"
#include "Optimize.h"
#include "SceneBuilder.h"

using namespace osg2vsg;

OSG::OSG()
{
    pipelineCache = osg2vsg::PipelineCache::create();
}

OSG::~OSG()
{
};

bool OSG::getFeatures(Features& features) const
{
    osgDB::FileNameList all_plugins = osgDB::listAllAvailablePlugins();
    osgDB::FileNameList plugins;
    for (auto& filename : all_plugins)
    {
        // the plugin list icludes the OSG's serializers so we need to discard these from being queried.
        if (filename.find("osgdb_serializers_") == std::string::npos && filename.find("osgdb_deprecated_") == std::string::npos)
        {
            plugins.push_back(filename);
        }
    }

    osgDB::ReaderWriterInfoList infoList;
    for (auto& pluginName : plugins)
    {
        osgDB::queryPlugin(pluginName, infoList);
    }

    const std::string dotPrefix = ".";
    for (auto& info : infoList)
    {
        for (auto& ext_description : info->extensions)
        {
            features.extensionFeatureMap[dotPrefix + ext_description.first] = vsg::ReaderWriter::READ_FILENAME;
        }
    }

    // enumerate the supported vsg::Options::setValue(str, value) options
    features.optionNameTypeMap[OSG::original_converter] = vsg::type_name<bool>();
    features.optionNameTypeMap[OSG::read_build_options] = vsg::type_name<std::string>();
    features.optionNameTypeMap[OSG::write_build_options] = vsg::type_name<std::string>();

    return true;
}


bool OSG::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(OSG::original_converter, &options);
    result = arguments.readAndAssign<std::string>(OSG::read_build_options, &options) || result;
    result = arguments.readAndAssign<std::string>(OSG::write_build_options, &options) || result;
    return result;
}

vsg::ref_ptr<vsg::Object> OSG::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    osg::ref_ptr<osgDB::Options> osg_options;

    if (options)
    {
        if (osgDB::Registry::instance()->getOptions())
            osg_options = osgDB::Registry::instance()->getOptions()->cloneOptions();
        else
            osg_options = new osgDB::Options();
        for (auto itr = options->paths.begin(); itr != options->paths.end(); ++itr)
            osg_options->getDatabasePathList().insert(osg_options->getDatabasePathList().end(), (*itr).string());
    }

    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext == ".path")
    {
        auto foundPath = vsg::findFile(filename, options);
        if (foundPath)
        {
            std::ifstream fin(foundPath);
            if (!fin) return {};

            osg::ref_ptr<osg::AnimationPath> osg_animationPath = new osg::AnimationPath;
            osg_animationPath->read(fin);

            auto vsg_animationPath = vsg::AnimationPath::create();

            switch (osg_animationPath->getLoopMode())
            {
            case (osg::AnimationPath::SWING):
                vsg_animationPath->mode = vsg::AnimationPath::FORWARD_AND_BACK;
                break;
            case (osg::AnimationPath::LOOP):
                vsg_animationPath->mode = vsg::AnimationPath::REPEAT;
                break;
            case (osg::AnimationPath::NO_LOOPING):
                vsg_animationPath->mode = vsg::AnimationPath::ONCE;
                break;
            }

            for (auto& [time, cp] : osg_animationPath->getTimeControlPointMap())
            {
                const auto& position = cp.getPosition();
                const auto& rotation = cp.getRotation();
                // const auto& scale = cp.getScale(); // TODO

                vsg_animationPath->add(time, vsg::dvec3(position.x(), position.y(), position.z()), vsg::dquat(rotation.x(), rotation.y(), rotation.z(), rotation.w()));
            }

            return vsg_animationPath;
        }
        return {};
    }

    osgDB::ReaderWriter::ReadResult rr = osgDB::Registry::instance()->readObject(filename.string(), osg_options.get());
    // if (!rr.success()) OSG_WARN << "Error reading file " << filename << ": " << rr.statusMessage() << std::endl;
    if (!rr.validObject()) return {};

    bool mapRGBtoRGBAHint = !options || options->mapRGBtoRGBAHint;

    osg::ref_ptr<osg::Object> object = rr.takeObject();
    if (osg::Node* osg_scene = object->asNode(); osg_scene != nullptr)
    {
        return osg2vsg::convert(*osg_scene, options);
    }
    else if (osg::Image* osg_image = dynamic_cast<osg::Image*>(object.get()); osg_image != nullptr)
    {
        return osg2vsg::convertToVsg(osg_image, mapRGBtoRGBAHint);
    }
    else if (osg::TransferFunction1D* tf = dynamic_cast<osg::TransferFunction1D*>(object.get()); tf != nullptr)
    {
        auto tf_image = tf->getImage();
        auto vsg_image = osg2vsg::convertToVsg(tf_image, mapRGBtoRGBAHint);
        vsg_image->setValue("Minimum", tf->getMinimum());
        vsg_image->setValue("Maximum", tf->getMaximum());

        return vsg_image;
    }
    else
    {
        vsg::warn("OSG::ImplementationreadFile(", filename, ") cannot convert object type ", object->className(), ")");
    }

    return {};
}
