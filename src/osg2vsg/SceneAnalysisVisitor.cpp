#include <osg2vsg/SceneAnalysisVisitor.h>

#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>

using namespace osg2vsg;

SceneAnalysisVisitor::SceneAnalysisVisitor():
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

osg::ref_ptr<osg::StateSet> SceneAnalysisVisitor::uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet)
{
    if (auto itr = uniqueStateSets.find(stateset); itr != uniqueStateSets.end())
    {
        std::cout<<"    uniqueState() found state"<<std::endl;
        return *itr;
    }

    std::cout<<"    uniqueState() inserting state"<<std::endl;


    if (stateset.valid())
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
            std::cout<<"Found program removing from dataState"<<program<<" inserting into programState"<<std::endl;
            dataState->removeAttribute(program);
            programState->setAttribute(program, attribute.second.second);
        }
    }

    return StatePair(uniqueState(programState, true), uniqueState(dataState, false));
}

void SceneAnalysisVisitor::apply(osg::Node& node)
{
    std::cout<<"Visiting "<<node.className()<<" "<<node.getStateSet()<<std::endl;

    if (node.getStateSet()) pushStateSet(*node.getStateSet());

    traverse(node);

    if (node.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::apply(osg::Group& group)
{
    std::cout<<"Group "<<group.className()<<" "<<group.getStateSet()<<std::endl;

    if (group.getStateSet()) pushStateSet(*group.getStateSet());

    traverse(group);

    if (group.getStateSet()) popStateSet();
}

void SceneAnalysisVisitor::apply(osg::Transform& transform)
{
    std::cout<<"Transform "<<transform.className()<<" "<<transform.getStateSet()<<std::endl;

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
    std::cout<<"apply(osg::Billboard& billboard)"<<std::endl;

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
    if (geometry.getStateSet()) pushStateSet(*geometry.getStateSet());

    auto itr = stateMap.find(statestack);

    if (itr==stateMap.end())
    {
        if (statestack.empty())
        {
            std::cout<<"New Empty StateSet's"<<std::endl;
            stateMap[statestack] = computeStatePair(0);
        }
        else if (statestack.size()==1)
        {
            std::cout<<"New Single  StateSet's"<<std::endl;
            stateMap[statestack] = computeStatePair(statestack.back());
        }
        else // multiple stateset's need to merge
        {
            std::cout<<"New Merging StateSet's "<<statestack.size()<<std::endl;
            osg::ref_ptr<osg::StateSet> new_stateset = new osg::StateSet;
            for(auto& stateset : statestack)
            {
                new_stateset->merge(*stateset);
            }
            stateMap[statestack] = computeStatePair(new_stateset);
        }

        itr = stateMap.find(statestack);

        std::cout<<"Need to create StateSet"<<std::endl;
    }
    else
    {
        std::cout<<"Already have StateSet"<<std::endl;
    }

    osg::Matrix matrix;
    if (!matrixstack.empty()) matrix = matrixstack.back();

    StatePair& statePair = itr->second;

    TransformStatePair& transformStatePair = programTransformStateMap[statePair.first];
    StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
    stateGeometryMap[statePair.second].push_back(&geometry);

    TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
    transformGeometryMap[matrix].push_back(&geometry);

    std::cout<<"   Geometry "<<geometry.className()<<" ss="<<statestack.size()<<" ms="<<matrixstack.size()<<std::endl;

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
    std::cout<<"\nprint()\n";
    std::cout<<"   programTransformStateMap.size() = "<<programTransformStateMap.size()<<std::endl;
    for(auto [programStateSet, transformStatePair] : programTransformStateMap)
    {
        std::cout<<"       programStateSet = "<<programStateSet.get()<<std::endl;
        std::cout<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
        std::cout<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
    }
}

osg::ref_ptr<osg::Node> SceneAnalysisVisitor::createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap)
{
    std::cout<<"createStateGeometryGraph()"<<stateGeometryMap.size()<<std::endl;

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
    std::cout<<"createStateGeometryGraph()"<<transformGeometryMap.size()<<std::endl;

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

        std::cout<<"       programStateSet = "<<programStateSet.get()<<std::endl;
        std::cout<<"           transformStatePair.matrixStateGeometryMap.size() = "<<transformStatePair.matrixStateGeometryMap.size()<<std::endl;
        std::cout<<"           transformStatePair.stateTransformMap.size() = "<<transformStatePair.stateTransformMap.size()<<std::endl;
    }
    return group;
}

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createStateGeometryGraphVSG(StateGeometryMap& stateGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask)
{
    std::cout << "createStateGeometryGraph()" << stateGeometryMap.size() << std::endl;

    if (stateGeometryMap.empty()) return vsg::ref_ptr<vsg::Node>();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();
    for (auto[stateset, geometries] : stateGeometryMap)
    {
        uint32_t statemask = calculateStateMask(stateset);
        statemask &= ~LIGHTING; // force lighting off until normal matrix is working

        vsg::ref_ptr<vsg::GraphicsPipelineGroup> graphicsPipelineGroup = createGeometryGraphicsPipeline(requiredGeomAttributesMask, statemask);
        group->addChild(graphicsPipelineGroup);

        auto stateGroup = vsg::Group::create();
        graphicsPipelineGroup->addChild(stateGroup);

        vsg::Group* attachpoint = stateGroup;

        // if we have a texture insert a texture node
        if (stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
        {
            /*std::string textureFile("textures/lz.vsgb");
            vsg::vsgReaderWriter vsgReader;
            auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));*/

            osg::Texture2D* osgtex = dynamic_cast<osg::Texture2D*>(stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

            if (osgtex && osgtex->getImage())
            {
                auto textureData = convertToVsg(osgtex->getImage());
                if (!textureData)
                {
                    std::cout << "Could not convert osg image data" << std::endl;
                    return vsg::ref_ptr<vsg::Node>();
                }
                vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
                texture->_textureData = textureData;

                attachpoint->addChild(texture);
                attachpoint = texture;
            }
        }

        for (auto& geometry : geometries)
        {
            vsg::ref_ptr<vsg::Geometry> new_geometry = convertToVsg(geometry, requiredGeomAttributesMask);
            if (new_geometry) attachpoint->addChild(new_geometry);
        }
    }

    if (group->getNumChildren() == 1) return vsg::ref_ptr<vsg::Node>(group->getChild(0));

    return group;
}

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask)
{
    std::cout << "createStateGeometryGraph()" << transformGeometryMap.size() << std::endl;

    if (transformGeometryMap.empty()) return vsg::ref_ptr<vsg::Node>();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();
    for (auto[matrix, geometries] : transformGeometryMap)
    {
        vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create();

        /*vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
                                        matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
                                        matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3),
                                        matrix(3, 0), matrix(3, 1), matrix(3, 2), matrix(3, 3));*/

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

vsg::ref_ptr<vsg::Node> SceneAnalysisVisitor::createVSG(vsg::Paths& searchPaths)
{
    uint32_t forceGeomAttributes = GeometryAttributes::STANDARD_ATTS;
    uint32_t forceStateMask = StateMask::DIFFUSE_MAP;

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

    for (auto[programStateSet, transformStatePair] : programTransformStateMap)
    {
        bool transformAtTop = transformStatePair.matrixStateGeometryMap.size() < transformStatePair.stateTransformMap.size();
        if (transformAtTop)
        {
            for (auto[matrix, stateGeometryMap] : transformStatePair.matrixStateGeometryMap)
            {
                vsg::ref_ptr<vsg::Node> stateGeometryGraph = createStateGeometryGraphVSG(stateGeometryMap, searchPaths, forceGeomAttributes);
                if (!stateGeometryGraph) continue;

                if (!matrix.isIdentity())
                {
                    vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create();

                    /*vsg::mat4 vsgmatrix = vsg::mat4(matrix(0,0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
                                                    matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
                                                    matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3),
                                                    matrix(3, 0), matrix(3, 1), matrix(3, 2), matrix(3, 3));*/

                    vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                                    matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                                    matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                                    matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));


                    transform->_matrix = new vsg::mat4Value(vsgmatrix);

                    group->addChild(transform);
                    transform->addChild(stateGeometryGraph);
                }
                else
                {
                    group->addChild(stateGeometryGraph);
                }
            }
        }
        else
        {
            for (auto[stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
            {
                vsg::ref_ptr<vsg::Node> transformGeometryGraph = createTransformGeometryGraphVSG(transformeGeometryMap, searchPaths, forceGeomAttributes);
                if (!transformGeometryGraph) continue;

                uint32_t statemask = calculateStateMask(stateset);
                statemask &= ~LIGHTING; // force lighting off until normal matrix is working

                vsg::ref_ptr<vsg::GraphicsPipelineGroup> graphicsPipelineGroup = createGeometryGraphicsPipeline(forceGeomAttributes, statemask);
                group->addChild(graphicsPipelineGroup);

                auto stateGroup = vsg::Group::create();
                graphicsPipelineGroup->addChild(stateGroup);

                vsg::Group* attachpoint = stateGroup;

                // if we have a texture insert a texture node
                if (stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
                {
                    /*std::string textureFile("textures/lz.vsgb");
                    vsg::vsgReaderWriter vsgReader;
                    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, searchPaths));*/

                    osg::Texture2D* osgtex = dynamic_cast<osg::Texture2D*>(stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

                    if (osgtex && osgtex->getImage())
                    {
                        auto textureData = convertToVsg(osgtex->getImage());
                        if (!textureData)
                        {
                            std::cout << "Could not convert osg image data" << std::endl;
                            return vsg::ref_ptr<vsg::Node>();
                        }
                        vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
                        texture->_textureData = textureData;

                        attachpoint->addChild(texture);
                        attachpoint = texture;
                    }
                }

                attachpoint->addChild(transformGeometryGraph);
            }
        }

        std::cout << "       programStateSet = " << programStateSet.get() << std::endl;
        std::cout << "           transformStatePair.matrixStateGeometryMap.size() = " << transformStatePair.matrixStateGeometryMap.size() << std::endl;
        std::cout << "           transformStatePair.stateTransformMap.size() = " << transformStatePair.stateTransformMap.size() << std::endl;
    }
    return group;
}
