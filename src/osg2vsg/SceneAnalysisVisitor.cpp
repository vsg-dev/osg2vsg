#include <osg2vsg/SceneAnalysisVisitor.h>

#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>

using namespace osg2vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif

SceneAnalysisVisitor::SceneAnalysisVisitor():
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN) {}

osg::ref_ptr<osg::StateSet> SceneAnalysisVisitor::uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet)
{
    if (auto itr = uniqueStateSets.find(stateset); itr != uniqueStateSets.end())
    {
        DEBUG_OUTPUT<<"    uniqueState() found state"<<std::endl;
        return *itr;
    }

    DEBUG_OUTPUT<<"    uniqueState() inserting state"<<std::endl;


    if (writeToFileProgramAndDataSetSets && stateset.valid())
    {
        if (programStateSet) osgDB::writeObjectFile(*(stateset), vsg::make_string("programState_", stateMap.size(),".osgt"));
        else osgDB::writeObjectFile(*(stateset), vsg::make_string("dataState_", stateMap.size(),".osgt"));
    }

    uniqueStateSets.insert(stateset);
    return stateset;
}

SceneAnalysisVisitor::StatePair SceneAnalysisVisitor::computeStatePair(osg::StateSet* stateset)
{
    if (!stateset) return StatePair();

    osg::ref_ptr<osg::StateSet> programState = new osg::StateSet;
    osg::ref_ptr<osg::StateSet> dataState = new osg::StateSet;

    programState->setModeList(stateset->getModeList());
    programState->setTextureModeList(stateset->getTextureModeList());
    programState->setDefineList(stateset->getDefineList());

    dataState->setAttributeList(stateset->getAttributeList());
    dataState->setTextureAttributeList(stateset->getTextureAttributeList());
    dataState->setUniformList(stateset->getUniformList());

    for(auto attribute : stateset->getAttributeList())
    {
        osg::Program* program = dynamic_cast<osg::Program*>(attribute.second.first.get());
        if (program)
        {
            DEBUG_OUTPUT<<"Found program removing from dataState"<<program<<" inserting into programState"<<std::endl;
            dataState->removeAttribute(program);
            programState->setAttribute(program, attribute.second.second);
        }
    }

    return StatePair(uniqueState(programState, true), uniqueState(dataState, false));
}

void SceneAnalysisVisitor::apply(osg::Node& node)
{
    DEBUG_OUTPUT<<"Visiting "<<node.className()<<" "<<node.getStateSet()<<std::endl;

    if (node.getStateSet()) pushStateSet(*node.getStateSet());

    traverse(node);

    if (node.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::apply(osg::Group& group)
{
    DEBUG_OUTPUT<<"Group "<<group.className()<<" "<<group.getStateSet()<<std::endl;

    if (group.getStateSet()) pushStateSet(*group.getStateSet());

    traverse(group);

    if (group.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::apply(osg::Transform& transform)
{
    DEBUG_OUTPUT<<"Transform "<<transform.className()<<" "<<transform.getStateSet()<<std::endl;

    if (transform.getStateSet()) pushStateSet(*transform.getStateSet());

    osg::Matrix matrix;
    if (!matrixstack.empty()) matrix = matrixstack.back();
    transform.computeLocalToWorldMatrix(matrix, this);

    pushMatrix(matrix);

    traverse(transform);

    popMatrix();

    if (transform.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::apply(osg::Billboard& billboard)
{
    DEBUG_OUTPUT<<"apply(osg::Billboard& billboard)"<<std::endl;

    for(unsigned int i=0; i<billboard.getNumDrawables(); ++i)
    {
        auto translate = osg::Matrixd::translate(billboard.getPosition(i));

        if (matrixstack.empty()) pushMatrix(translate);
        else pushMatrix(translate * matrixstack.back());

        billboard.getDrawable(i)->accept(*this);

        popMatrix();
    }
}

void SceneAnalysisVisitor::apply(osg::Geometry& geometry)
{
    if (!geometry.getVertexArray())
    {
        DEBUG_OUTPUT<<"SceneAnalysisVisitor::apply(osg::Geometry& geometry), ignoring geometry with null geometry.getVertexArray()"<<std::endl;
        return;
    }

    if (geometry.getVertexArray()->getNumElements()==0)
    {
        DEBUG_OUTPUT<<"SceneAnalysisVisitor::apply(osg::Geometry& geometry), ignoring geometry with empty geometry.getVertexArray()"<<std::endl;
        return;
    }

    if (geometry.getNumPrimitiveSets()==0)
    {
        DEBUG_OUTPUT<<"SceneAnalysisVisitor::apply(osg::Geometry& geometry), ignoring geometry with empty PrimitiveSetList."<<std::endl;
        return;
    }

    for(auto& primitive : geometry.getPrimitiveSetList())
    {
        if (primitive->getNumPrimitives()==0)
        {
            DEBUG_OUTPUT<<"SceneAnalysisVisitor::apply(osg::Geometry& geometry), ignoring geometry with as it contains an empty PrimitiveSet : "<<primitive->className()<<std::endl;
            return;
        }
    }

    if (geometry.getStateSet()) pushStateSet(*geometry.getStateSet());

    auto itr = stateMap.find(statestack);

    if (itr==stateMap.end())
    {
        if (statestack.empty())
        {
            DEBUG_OUTPUT<<"New Empty StateSet's"<<std::endl;
            stateMap[statestack] = computeStatePair(0);
        }
        else if (statestack.size()==1)
        {
            DEBUG_OUTPUT<<"New Single  StateSet's"<<std::endl;
            stateMap[statestack] = computeStatePair(statestack.back());
        }
        else // multiple stateset's need to merge
        {
            DEBUG_OUTPUT<<"New Merging StateSet's "<<statestack.size()<<std::endl;
            osg::ref_ptr<osg::StateSet> new_stateset = new osg::StateSet;
            for(auto& stateset : statestack)
            {
                new_stateset->merge(*stateset);
            }
            stateMap[statestack] = computeStatePair(new_stateset);
        }

        itr = stateMap.find(statestack);

        DEBUG_OUTPUT<<"Need to create StateSet"<<std::endl;
    }
    else
    {
        DEBUG_OUTPUT<<"Already have StateSet"<<std::endl;
    }

    osg::Matrix matrix;
    if (!matrixstack.empty()) matrix = matrixstack.back();

    // Build programTransformStateMap
    {
        StatePair& statePair = itr->second;

        TransformStatePair& transformStatePair = programTransformStateMap[statePair.first];
        StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
        stateGeometryMap[statePair.second].push_back(&geometry);

        TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
        transformGeometryMap[matrix].push_back(&geometry);
    }

    // Build new masksTransformStateMap
    {
        StatePair& statePair = itr->second;
        Masks masks(calculateShaderModeMask(statePair.first.get()) | calculateShaderModeMask(statePair.second.get()), calculateAttributesMask(&geometry));

        DEBUG_OUTPUT<<"populating masks ("<<masks.first<<", "<<masks.second<<")"<<std::endl;

        TransformStatePair& transformStatePair = masksTransformStateMap[masks];
        StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
        stateGeometryMap[statePair.second].push_back(&geometry);

        TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
        transformGeometryMap[matrix].push_back(&geometry);
    }

    DEBUG_OUTPUT<<"   Geometry "<<geometry.className()<<" ss="<<statestack.size()<<" ms="<<matrixstack.size()<<std::endl;

    if (geometry.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::pushStateSet(osg::StateSet& stateset)
{
    statestack.push_back(&stateset);
}

void SceneAnalysisVisitor::popStateSet()
{
    statestack.pop_back();
}

void SceneAnalysisVisitor::SceneAnalysisVisitor::pushMatrix(const osg::Matrix& matrix)
{
    matrixstack.push_back(matrix);
}

void SceneAnalysisVisitor::popMatrix()
{
    matrixstack.pop_back();
}

void SceneAnalysisVisitor::print()
{
    DEBUG_OUTPUT<<"\nprint()\n";
    DEBUG_OUTPUT<<"   programTransformStateMap.size() = "<<programTransformStateMap.size()<<std::endl;
    for(auto [programStateSet, transformStatePair] : programTransformStateMap)
    {
        DEBUG_OUTPUT<<"       programStateSet = "<<programStateSet.get()<<std::endl;
        DEBUG_OUTPUT<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
        DEBUG_OUTPUT<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
    }
}

osg::ref_ptr<osg::Node> SceneAnalysisVisitor::createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap)
{
    DEBUG_OUTPUT<<"createStateGeometryGraph()"<<stateGeometryMap.size()<<std::endl;

    if (stateGeometryMap.empty()) return nullptr;

    osg::ref_ptr<osg::Group> group = new osg::Group;
    for(auto [stateset, geometries] : stateGeometryMap)
    {
        osg::ref_ptr<osg::Group> stateGroup = new osg::Group;
        stateGroup->setStateSet(stateset);
        group->addChild(stateGroup);

        for(auto& geometry : geometries)
        {
            osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
            new_geometry->setStateSet(nullptr);
            stateGroup->addChild(new_geometry);
        }
    }

    if (group->getNumChildren()==1) return group->getChild(0);

    return group;
}

osg::ref_ptr<osg::Node> SceneAnalysisVisitor::createTransformGeometryGraphOSG(TransformGeometryMap& transformGeometryMap)
{
    DEBUG_OUTPUT<<"createStateGeometryGraph()"<<transformGeometryMap.size()<<std::endl;

    if (transformGeometryMap.empty()) return nullptr;

    osg::ref_ptr<osg::Group> group = new osg::Group;
    for(auto [matrix, geometries] : transformGeometryMap)
    {
        osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
        transform->setMatrix(matrix);
        group->addChild(transform);

        for(auto& geometry : geometries)
        {
            osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
            new_geometry->setStateSet(nullptr);
            transform->addChild(new_geometry);
        }
    }

    if (group->getNumChildren()==1) return group->getChild(0);

    return group;
}

osg::ref_ptr<osg::Node> SceneAnalysisVisitor::createOSG()
{
    osg::ref_ptr<osg::Group> group = new osg::Group;

    for(auto [programStateSet, transformStatePair] : programTransformStateMap)
    {
        osg::ref_ptr<osg::Group> programGroup = new osg::Group;
        group->addChild(programGroup);
        programGroup->setStateSet(programStateSet.get());

        bool transformAtTop = transformStatePair.matrixStateGeometryMap.size() < transformStatePair.stateTransformMap.size();
        if (transformAtTop)
        {
            for(auto [matrix, stateGeometryMap] : transformStatePair.matrixStateGeometryMap)
            {
                osg::ref_ptr<osg::Node> stateGeometryGraph = createStateGeometryGraphOSG(stateGeometryMap);
                if (!stateGeometryGraph) continue;

                if (!matrix.isIdentity())
                {
                    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
                    transform->setMatrix(matrix);
                    programGroup->addChild(transform);
                    transform->addChild(stateGeometryGraph);
                }
                else
                {
                    programGroup->addChild(stateGeometryGraph);
                }
            }
        }
        else
        {
            for(auto [stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
            {
                osg::ref_ptr<osg::Node> transformGeometryGraph = createTransformGeometryGraphOSG(transformeGeometryMap);
                if (!transformGeometryGraph) continue;

                transformGeometryGraph->setStateSet(stateset);
                programGroup->addChild(transformGeometryGraph);
            }
        }

        DEBUG_OUTPUT<<"       programStateSet = "<<programStateSet.get()<<std::endl;
        DEBUG_OUTPUT<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
        DEBUG_OUTPUT<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
    }
    return group;
}

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask)
{
    DEBUG_OUTPUT << "createStateGeometryGraph()" << transformGeometryMap.size() << std::endl;

    if (transformGeometryMap.empty()) return vsg::ref_ptr<vsg::Node>();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();
    for (auto[matrix, geometries] : transformGeometryMap)
    {
        vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create();

        vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                        matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                        matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                        matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));

        transform->_matrix = new vsg::mat4Value(vsgmatrix);

        group->addChild(transform);

        for (auto& geometry : geometries)
        {
            vsg::ref_ptr<vsg::Geometry> new_geometry = convertToVsg(geometry, requiredGeomAttributesMask); // new osg::Geometry(*geometry);
            if (new_geometry) transform->addChild(new_geometry);
        }
    }

    if (group->getNumChildren() == 1) return vsg::ref_ptr<vsg::Node>(group->getChild(0));

    return group;
}

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createNewVSG(vsg::Paths& searchPaths)
{
    DEBUG_OUTPUT<<"SceneAnalysisVisitor::createNewVSG(vsg::Paths& searchPaths)"<<std::endl;

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

    for (auto[masks, transformStatePair] : masksTransformStateMap)
    {
        uint32_t geometrymask = masks.second;
        uint32_t shaderModeMask = masks.first;

        shaderModeMask |= LIGHTING; // force lighting on
        if(shaderModeMask & NORMAL_MAP) geometrymask |= TANGENT; // mesh propably won't have tangets so force them on if we want Normal mapping

        unsigned int maxNumDescriptors = transformStatePair.stateTransformMap.size();

        auto graphicsPipelineGroup = createGeometryGraphicsPipeline(shaderModeMask, geometrymask, maxNumDescriptors, vertexShaderPath, fragmentShaderPath);

        group->addChild(graphicsPipelineGroup);

        for (auto[stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
        {
            vsg::ref_ptr<vsg::Node> transformGeometryGraph = createTransformGeometryGraphVSG(transformeGeometryMap, searchPaths, geometrymask);
            if (!transformGeometryGraph) continue;

            vsg::ref_ptr<vsg::AttributesNode> attributesNode = createTextureAttributesNode(stateset);
            if (attributesNode)
            {
                graphicsPipelineGroup->addChild(attributesNode);
                attributesNode->addChild(transformGeometryGraph);
            }
            else
            {
                graphicsPipelineGroup->addChild(transformGeometryGraph);
            }
        }
    }
    return group;
}

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createCoreVSG(vsg::Paths& searchPaths)
{
    std::cout<<"SceneAnalysisVisitor::createCoreVSG(vsg::Paths& searchPaths)"<<std::endl;

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

    for (auto[masks, transformStatePair] : masksTransformStateMap)
    {
        unsigned int maxNumDescriptors = transformStatePair.stateTransformMap.size();
        if (maxNumDescriptors==0)
        {
            std::cout<<"  Skipping empty transformStatePair"<<std::endl;
            continue;
        }
        else
        {
            std::cout<<"  maxNumDescriptors = "<<maxNumDescriptors<<std::endl;
        }

        uint32_t geometrymask = (masks.second | overrideGeomAttributes) & supportedGeometryAttributes;
        uint32_t shaderModeMask = (masks.first | overrideShaderModeMask) & supportedShaderModeMask;
        if (shaderModeMask & NORMAL_MAP) geometrymask |= TANGENT; // mesh propably won't have tangets so force them on if we want Normal mapping

        std::cout<<"  about to call createStateSetWithGraphicsPipeline("<<shaderModeMask<<", "<<geometrymask<<", "<<maxNumDescriptors<<")"<<std::endl;

        auto graphicsPipelineGroup = vsg::StateGroup::create();
        graphicsPipelineGroup->add(createGraphicsPipelineAttribute(shaderModeMask, geometrymask, maxNumDescriptors, vertexShaderPath, fragmentShaderPath));

        group->addChild(graphicsPipelineGroup);

        for (auto[stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
        {
            vsg::ref_ptr<vsg::Node> transformGeometryGraph = createTransformGeometryGraphVSG(transformeGeometryMap, searchPaths, geometrymask);
            if (!transformGeometryGraph) continue;

            vsg::ref_ptr<vsg::StateSet> vsg_stateset = createVsgStateSet(stateset, shaderModeMask);
            if (vsg_stateset)
            {
                auto stategroup = vsg::StateGroup::create();

                graphicsPipelineGroup->addChild(stategroup);
                stategroup->add(vsg_stateset);
                stategroup->addChild(transformGeometryGraph);
            }
            else
            {
                graphicsPipelineGroup->addChild(transformGeometryGraph);
            }
        }
    }
    return group;
}
