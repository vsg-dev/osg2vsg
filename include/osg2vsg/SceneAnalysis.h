#pragma once

#include <vsg/all.h>

#include <osg/Geometry>

#include <typeindex>


namespace osg2vsg
{
    struct SceneStats : public vsg::Object
    {
        using ObjectFrequencyMap = std::map<void*, uint32_t>;
        using TypeFrequencyMap = std::map<const char*, ObjectFrequencyMap>;

        TypeFrequencyMap typeFrequencyMap;

        template<typename T>
        void insert(T* object)
        {
            if (object)
            {
                ++typeFrequencyMap[object->className()][object];
            }
        }


        void print(std::ostream& out);
    };


    class OsgSceneAnalysis : public osg::NodeVisitor
    {
    public:

        vsg::ref_ptr<SceneStats> _sceneStats;

        OsgSceneAnalysis();
        OsgSceneAnalysis(SceneStats* sceneStats);

        void apply(osg::Node& node);
        void apply(osg::Geometry& geometry);
        void apply(osg::StateSet& stateset);
    };

}
