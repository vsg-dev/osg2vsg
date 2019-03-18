#include <osg2vsg/SceneAnalysis.h>


using namespace osg2vsg;

void SceneStats::print(std::ostream& out)
{
    out<<"SceneStats class count: "<<typeFrequencyMap.size()<<"\n";
    size_t longestClassName = 0;
    for(auto& entry : typeFrequencyMap)
    {
        longestClassName = std::max(strlen(entry.first), longestClassName);
    }


    out<<"\nUnique objects:\n";
    out<<"    className";
    for(size_t i = strlen("className"); i<longestClassName; ++i) out<<" ";
    out<<"\tObjects\n";

    for(auto& [className, objectFrequencyMap] : typeFrequencyMap)
    {
        uint32_t totalInstances = 0;
        for(auto& entry : objectFrequencyMap)
        {
            totalInstances += entry.second;
        }

        if (objectFrequencyMap.size() == totalInstances)
        {
            out<<"    "<<className<<"";
            for(size_t i = strlen(className); i<longestClassName; ++i) out<<" ";
            out<<"\t"<<objectFrequencyMap.size()<<"\n";
        }
    }

    out<<"\nShared objects:\n";
    out<<"    className";
    for(size_t i = strlen("className"); i<longestClassName; ++i) out<<" ";
    out<<"\tObjects\tInstances\n";

    for(auto& [className, objectFrequencyMap] : typeFrequencyMap)
    {
        uint32_t totalInstances = 0;
        for(auto& entry : objectFrequencyMap)
        {
            totalInstances += entry.second;
        }

        if (objectFrequencyMap.size() != totalInstances)
        {
            out<<"    "<<className<<"";
            for(size_t i = strlen(className); i<longestClassName; ++i) out<<" ";
            out<<"\t"<<objectFrequencyMap.size()<<"\t"<<totalInstances<<"\n";
        }
    }
    out<<std::endl;
}



OsgSceneAnalysis::OsgSceneAnalysis() :
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _sceneStats(new SceneStats) {}

OsgSceneAnalysis::OsgSceneAnalysis(SceneStats* sceneStats) :
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _sceneStats(sceneStats) {}

void OsgSceneAnalysis::apply(osg::Node& node)
{
    if (node.getStateSet()) apply(*node.getStateSet());

    _sceneStats->insert(&node);

    node.traverse(*this);
}

void OsgSceneAnalysis::apply(osg::Geometry& geometry)
{
    if (geometry.getStateSet()) apply(*geometry.getStateSet());

    _sceneStats->insert(&geometry);

    osg::Geometry::ArrayList arrayList;
    geometry.getArrayList(arrayList);
    for(auto& array : arrayList)
    {
        _sceneStats->insert(array.get());
    }

    for(auto& primitiveSet : geometry.getPrimitiveSetList())
    {
        _sceneStats->insert(primitiveSet.get());
    }

}

void OsgSceneAnalysis::apply(osg::StateSet& stateset)
{
    _sceneStats->insert(&stateset);

    for(auto& attribute : stateset.getAttributeList())
    {
        _sceneStats->insert(attribute.second.first.get());
    }

    for(auto& textureList : stateset.getTextureAttributeList())
    {
        for(auto& attribute : textureList)
        {
            _sceneStats->insert(attribute.second.first.get());
        }
    }

    for(auto& uniform : stateset.getUniformList())
    {
        _sceneStats->insert(uniform.second.first.get());
    }
}
