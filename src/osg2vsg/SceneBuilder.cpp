#include <osg2vsg/SceneBuilder.h>

#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>

#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/CullNode.h>

using namespace osg2vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif

SceneBuilder::SceneBuilder():
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN) {}

osg::ref_ptr<osg::StateSet> SceneBuilder::uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet)
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

SceneBuilder::StatePair SceneBuilder::computeStatePair(osg::StateSet* stateset)
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

void SceneBuilder::apply(osg::Node& node)
{
    DEBUG_OUTPUT<<"Visiting "<<node.className()<<" "<<node.getStateSet()<<std::endl;

    if (node.getStateSet()) pushStateSet(*node.getStateSet());

    traverse(node);

    if (node.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Group& group)
{
    DEBUG_OUTPUT<<"Group "<<group.className()<<" "<<group.getStateSet()<<std::endl;

    if (group.getStateSet()) pushStateSet(*group.getStateSet());

    traverse(group);

    if (group.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Transform& transform)
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

void SceneBuilder::apply(osg::Billboard& billboard)
{
    DEBUG_OUTPUT<<"apply(osg::Billboard& billboard)"<<std::endl;

    if (billboard.getStateSet()) pushStateSet(*billboard.getStateSet());

    nodeShaderModeMasks = BILLBOARD;

    for(unsigned int i=0; i<billboard.getNumDrawables(); ++i)
    {
        auto translate = osg::Matrixd::translate(billboard.getPosition(i));

        if (matrixstack.empty()) pushMatrix(translate);
        else pushMatrix(translate * matrixstack.back());

        billboard.getDrawable(i)->accept(*this);

        popMatrix();
    }

    nodeShaderModeMasks = NONE;

    if (billboard.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Geometry& geometry)
{
    if (!geometry.getVertexArray())
    {
        DEBUG_OUTPUT<<"SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with null geometry.getVertexArray()"<<std::endl;
        return;
    }

    if (geometry.getVertexArray()->getNumElements()==0)
    {
        DEBUG_OUTPUT<<"SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with empty geometry.getVertexArray()"<<std::endl;
        return;
    }

    if (geometry.getNumPrimitiveSets()==0)
    {
        DEBUG_OUTPUT<<"SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with empty PrimitiveSetList."<<std::endl;
        return;
    }

    for(auto& primitive : geometry.getPrimitiveSetList())
    {
        if (primitive->getNumPrimitives()==0)
        {
            DEBUG_OUTPUT<<"SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with as it contains an empty PrimitiveSet : "<<primitive->className()<<std::endl;
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
        Masks masks(calculateShaderModeMask(statePair.first.get()) | calculateShaderModeMask(statePair.second.get()) | nodeShaderModeMasks, calculateAttributesMask(&geometry));

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

void SceneBuilder::pushStateSet(osg::StateSet& stateset)
{
    statestack.push_back(&stateset);
}

void SceneBuilder::popStateSet()
{
    statestack.pop_back();
}

void SceneBuilder::SceneBuilder::pushMatrix(const osg::Matrix& matrix)
{
    matrixstack.push_back(matrix);
}

void SceneBuilder::popMatrix()
{
    matrixstack.pop_back();
}

void SceneBuilder::print()
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

osg::ref_ptr<osg::Node> SceneBuilder::createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap)
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

osg::ref_ptr<osg::Node> SceneBuilder::createTransformGeometryGraphOSG(TransformGeometryMap& transformGeometryMap)
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

osg::ref_ptr<osg::Node> SceneBuilder::createOSG()
{
    // clear caches
    geometriesMap.clear();
    texturesMap.clear();

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

vsg::ref_ptr<vsg::Node> SceneBuilder::createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& /*searchPaths*/, uint32_t requiredGeomAttributesMask)
{
    DEBUG_OUTPUT << "createTransformGeometryGraphVSG() " << transformGeometryMap.size() << std::endl;

    if (transformGeometryMap.empty()) return vsg::ref_ptr<vsg::Node>();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();
    for (auto[matrix, geometries] : transformGeometryMap)
    {
        vsg::ref_ptr<vsg::Group> localGroup = group;

        bool requiresTransform = !matrix.isIdentity();

#if 1
        bool requiresTopCullGroup = (insertCullGroups || insertCullNodes) && (requiresTransform/* || geometries.size()==1*/);
        bool requiresLeafCullGroup = (insertCullGroups || insertCullNodes) && !requiresTopCullGroup;
        if (requiresTransform)
        {
            // need to insert a transform
            vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                            matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                            matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                            matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));

            vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create(vsgmatrix);

            localGroup = transform;


            if (insertCullGroups || insertCullNodes)
            {
                osg::BoundingBox overall_bb;
                for (auto& geometry : geometries)
                {
                    osg::BoundingBox bb = geometry->getBoundingBox();
                    for(int i=0; i<8; ++i)
                    {
                        overall_bb.expandBy(bb.corner(i) * matrix);
                    }
                }

                vsg::vec3 bb_min(overall_bb.xMin(), overall_bb.yMin(), overall_bb.zMin());
                vsg::vec3 bb_max(overall_bb.xMax(), overall_bb.yMax(), overall_bb.zMax());
                vsg::sphere boundingSphere((bb_min + bb_max)*0.5f, vsg::length(bb_max - bb_min)*0.5f);

                if (insertCullNodes)
                {
                    group->addChild(vsg::CullNode::create(boundingSphere, transform));
                }
                else
                {
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    cullGroup->addChild(transform);
                    group->addChild(cullGroup);
                }
            }
            else
            {
                group->addChild(transform);
            }

        }
#else
        bool requiresTopCullGroup = (insertCullGroups || insertCullNodes) && (requiresTransform || geometries.size()==1);
        bool requiresLeafCullGroup = (insertCullGroups || insertCullNodes) && !requiresTopCullGroup;
        if (requiresTopCullGroup)
        {
            osg::BoundingBox overall_bb;
            for (auto& geometry : geometries)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                for(int i=0; i<8; ++i)
                {
                    overall_bb.expandBy(bb.corner(i) * matrix);
                }
            }

            vsg::vec3 bb_min(overall_bb.xMin(), overall_bb.yMin(), overall_bb.zMin());
            vsg::vec3 bb_max(overall_bb.xMax(), overall_bb.yMax(), overall_bb.zMax());

            vsg::sphere boundingSphere((bb_min + bb_max)*0.5f, vsg::length(bb_max - bb_min)*0.5f);
            auto cullGroup = vsg::CullGroup::create(boundingSphere);
            localGroup->addChild(cullGroup);
            localGroup = cullGroup;
        }


        if (requiresTransform)
        {
            // need to insert a transform
            vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                            matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                            matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                            matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));

            vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create(vsgmatrix);

            localGroup->addChild(transform);

            localGroup = transform;
        }
#endif

        for (auto& geometry : geometries)
        {
#if 1
            vsg::ref_ptr<vsg::Command> leaf;
            if(geometriesMap.find(geometry) != geometriesMap.end())
            {
                DEBUG_OUTPUT << "sharing geometry" << std::endl;
                leaf = geometriesMap[geometry];
            }
            else
            {
                leaf = convertToVsg(geometry, requiredGeomAttributesMask, geometryTarget);
                if (leaf)
                {
                    geometriesMap[geometry] = leaf;
                }
            }

            if (requiresLeafCullGroup)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                vsg::vec3 bb_min(bb.xMin(), bb.yMin(), bb.zMin());
                vsg::vec3 bb_max(bb.xMax(), bb.yMax(), bb.zMax());

                vsg::sphere boundingSphere((bb_min + bb_max)*0.5f, vsg::length(bb_max - bb_min)*0.5f);
                if (insertCullNodes)
                {
                    std::cout<<"Using CullNode"<<std::endl;
                    localGroup->addChild( vsg::CullNode::create(boundingSphere, leaf) );
                }
                else
                {
                    std::cout<<"Using CullGroupe"<<std::endl;
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    cullGroup->addChild(leaf);
                    localGroup->addChild(cullGroup);
                }
            }
            else
            {
                localGroup->addChild(leaf);
            }
#else
            vsg::ref_ptr<vsg::Group> nestedGroup = localGroup;

            if (requiresLeafCullGroup)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                vsg::vec3 bb_min(bb.xMin(), bb.yMin(), bb.zMin());
                vsg::vec3 bb_max(bb.xMax(), bb.yMax(), bb.zMax());

                vsg::sphere boundingSphere((bb_min + bb_max)*0.5f, vsg::length(bb_max - bb_min)*0.5f);
                if (insertCullNodes)
                {
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    nestedGroup->addChild(cullGroup);
                    nestedGroup = cullGroup;
                }
            }

            // has the geometry already been converted
            if(geometriesMap.find(geometry) != geometriesMap.end())
            {
                DEBUG_OUTPUT << "sharing geometry" << std::endl;
                nestedGroup->addChild(vsg::ref_ptr<vsg::Node>(geometriesMap[geometry]));
            }
            else
            {
                vsg::ref_ptr<vsg::Geometry> new_geometry = convertToVsg(geometry, requiredGeomAttributesMask);
                if (new_geometry)
                {
                    nestedGroup->addChild(new_geometry);
                    geometriesMap[geometry] = new_geometry;
                }
            }
#endif
        }
    }

    if (group->getNumChildren() == 1) return vsg::ref_ptr<vsg::Node>(group->getChild(0));

    return group;
}

vsg::ref_ptr<vsg::Node> SceneBuilder::createVSG(vsg::Paths& searchPaths)
{
    DEBUG_OUTPUT<<"SceneBuilder::createVSG(vsg::Paths& searchPaths)"<<std::endl;

    // clear caches
    geometriesMap.clear();
    texturesMap.clear();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

    vsg::ref_ptr<vsg::Group> opaqueGroup = vsg::Group::create();
    group->addChild(opaqueGroup);

    vsg::ref_ptr<vsg::Group> transparentGroup = vsg::Group::create();
    group->addChild(transparentGroup);

    for (auto[masks, transformStatePair] : masksTransformStateMap)
    {
        unsigned int maxNumDescriptors = transformStatePair.stateTransformMap.size();
        if (maxNumDescriptors==0)
        {
            DEBUG_OUTPUT<<"  Skipping empty transformStatePair"<<std::endl;
            continue;
        }
        else
        {
            DEBUG_OUTPUT<<"  maxNumDescriptors = "<<maxNumDescriptors<<std::endl;
        }

        uint32_t geometrymask = (masks.second | overrideGeomAttributes) & supportedGeometryAttributes;
        uint32_t shaderModeMask = (masks.first | overrideShaderModeMask) & supportedShaderModeMask;
        if (shaderModeMask & NORMAL_MAP) geometrymask |= TANGENT; // mesh propably won't have tangets so force them on if we want Normal mapping

        DEBUG_OUTPUT<<"  about to call createStateSetWithGraphicsPipeline("<<shaderModeMask<<", "<<geometrymask<<", "<<maxNumDescriptors<<")"<<std::endl;

        auto graphicsPipelineGroup = vsg::StateGroup::create();

        auto bindGraphicsPipeline = createBindGraphicsPipeline(shaderModeMask, geometrymask, vertexShaderPath, fragmentShaderPath);
        graphicsPipelineGroup->add(bindGraphicsPipeline);

        auto graphicsPipeline = bindGraphicsPipeline->getPipeline();
        auto& descriptorSetLayouts = graphicsPipeline->getPipelineLayout()->getDescriptorSetLayouts();

        // attach based on use of transparency
        if(shaderModeMask & BLEND)
        {
            transparentGroup->addChild(graphicsPipelineGroup);
        }
        else
        {
            opaqueGroup->addChild(graphicsPipelineGroup);
        }

        for (auto[stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
        {
            vsg::ref_ptr<vsg::Node> transformGeometryGraph = createTransformGeometryGraphVSG(transformeGeometryMap, searchPaths, geometrymask);
            if (!transformGeometryGraph) continue;

            vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = createVsgStateSet(descriptorSetLayouts, stateset, shaderModeMask);
            if (descriptorSet)
            {
                auto stategroup = vsg::StateGroup::create();
                graphicsPipelineGroup->addChild(stategroup);
                stategroup->addChild(transformGeometryGraph);

                if (useBindDescriptorSet)
                {
                    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, descriptorSet);
                    stategroup->add(bindDescriptorSet);
                }
                else
                {
                    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});
                    stategroup->add(bindDescriptorSets);
                }
            }
            else
            {
                graphicsPipelineGroup->addChild(transformGeometryGraph);
            }
        }
    }


    // if we are using CullGroups then place one at the top of the created scene graph
    if (insertCullGroups)
    {
        vsg::ComputeBounds computeBounds;
        group->accept(computeBounds);

        vsg::sphere boundingSphere((computeBounds.bounds.min+computeBounds.bounds.max)*0.5, vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.5);
        auto cullGroup = vsg::CullGroup::create(boundingSphere);

        // add the groups children to the cullGroup
        cullGroup->setChildren(group->getChildren());

        // now use the cullGroup as the root.
        group = cullGroup;
    }

    return group;
}

vsg::ref_ptr<vsg::Texture> SceneBuilder::convertToVsgTexture(const osg::Texture* osgtexture)
{
    if (auto itr = texturesMap.find(osgtexture); itr != texturesMap.end()) return itr->second;

    const osg::Image* image = osgtexture ? osgtexture->getImage(0) : nullptr;
    auto textureData = convertToVsg(image);
    if (!textureData)
    {
        // DEBUG_OUTPUT << "Could not convert osg image data" << std::endl;
        return vsg::ref_ptr<vsg::Texture>();
    }
    vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
    texture->_textureData = textureData;
    texture->_samplerInfo = convertToSamplerCreateInfo(osgtexture);

    texturesMap[osgtexture] = texture;

    return texture;
}

vsg::ref_ptr<vsg::DescriptorSet> SceneBuilder::createVsgStateSet(const vsg::DescriptorSetLayouts& descriptorSetLayouts, const osg::StateSet* stateset, uint32_t shaderModeMask)
{
    if (!stateset) return vsg::ref_ptr<vsg::DescriptorSet>();

    uint32_t texcount = 0;

    vsg::Descriptors descriptors;

    auto addTexture = [&] (unsigned int i)
    {
        const osg::StateAttribute* texatt = stateset->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
        const osg::Texture* osgtex = dynamic_cast<const osg::Texture*>(texatt);
        if (osgtex)
        {
            vsg::ref_ptr<vsg::Texture> vsgtex = convertToVsgTexture(osgtex);
            if (vsgtex)
            {
                // shaders are looking for textures in original units
                vsgtex->_dstBinding = i;
                texcount++; //
                descriptors.push_back(vsgtex);
            }
            else
            {
                std::cout<<"createVsgStateSet(..) osg::Texture, with i="<<i<<" found but cannot be mapped to vsg::Texture."<<std::endl;
            }
        }
    };

    // add material first
    const osg::Material* osg_material = dynamic_cast<const osg::Material*>(stateset->getAttribute(osg::StateAttribute::Type::MATERIAL));
    if (osg_material != nullptr && stateset->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON)
    {
        vsg::ref_ptr<vsg::MaterialValue> matdata = convertToMaterialValue(osg_material);
        vsg::ref_ptr<vsg::Uniform> vsg_materialUniform = vsg::Uniform::create();
        vsg_materialUniform->_dataList.push_back(matdata);
        vsg_materialUniform->_dstBinding = 10; // just use high value for now, should maybe put uniforms into a different descriptor set to simplify binding indexes
        descriptors.push_back(vsg_materialUniform);
    }

    // add textures
    if (shaderModeMask & ShaderModeMask::DIFFUSE_MAP) addTexture(DIFFUSE_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::OPACITY_MAP) addTexture(OPACITY_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::AMBIENT_MAP) addTexture(AMBIENT_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::NORMAL_MAP) addTexture(NORMAL_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::SPECULAR_MAP) addTexture(SPECULAR_TEXTURE_UNIT);

    if (descriptors.size() == 0) return vsg::ref_ptr<vsg::DescriptorSet>();

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, descriptors);

    return descriptorSet;
}


vsg::ref_ptr<vsg::BindGraphicsPipeline> SceneBuilder::createBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryAttributesMask, const std::string& vertShaderPath, const std::string& fragShaderPath)
{
    vsg::ShaderModules shaders{
        vsg::ShaderModule::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertShaderPath.empty() ? createFbxVertexSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(vertShaderPath, shaderModeMask, geometryAttributesMask)),
        vsg::ShaderModule::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderPath.empty() ? createFbxFragmentSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(fragShaderPath, shaderModeMask, geometryAttributesMask))
    };

    if (!shaderCompiler.compile(shaders)) return vsg::ref_ptr<vsg::BindGraphicsPipeline>();

    // std::cout<<"createBindGraphicsPipeline("<<shaderModeMask<<", "<<geometryAttributesMask<<")"<<std::endl;

    vsg::DescriptorSetLayoutBindings descriptorBindings;

    // add material first if any (for now material is hardcoded to binding 10)
    if (shaderModeMask & MATERIAL) descriptorBindings.push_back({ 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}

    // these need to go in incremental order by texture unit value as that how they will have been added to the desctiptor set
    // VkDescriptorSetLayoutBinding { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    if (shaderModeMask & DIFFUSE_MAP) descriptorBindings.push_back({ DIFFUSE_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    if (shaderModeMask & OPACITY_MAP) descriptorBindings.push_back({ OPACITY_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
    if (shaderModeMask & AMBIENT_MAP) descriptorBindings.push_back({ AMBIENT_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
    if (shaderModeMask & NORMAL_MAP) descriptorBindings.push_back({ NORMAL_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
    if (shaderModeMask & SPECULAR_MAP) descriptorBindings.push_back({ SPECULAR_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

    vsg::PushConstantRanges pushConstantRanges
    {
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
    };

    uint32_t vertexBindingIndex = 0;

    vsg::VertexInputState::Bindings vertexBindingsDescriptions = vsg::VertexInputState::Bindings
    {
        VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions = vsg::VertexInputState::Attributes
    {
        VkVertexInputAttributeDescription{VERTEX_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
    };

    vertexBindingIndex++;

    if (geometryAttributesMask & NORMAL)
    {
        VkVertexInputRate nrate = geometryAttributesMask & NORMAL_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec3), nrate});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ NORMAL_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0 }); // normal as vec3
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & TANGENT)
    {
        VkVertexInputRate trate = geometryAttributesMask & TANGENT_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec4), trate });
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ TANGENT_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }); // tanget as vec4
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & COLOR)
    {
        VkVertexInputRate crate = geometryAttributesMask & COLOR_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec4), crate });
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ COLOR_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }); // color as vec4
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & TEXCOORD0)
    {
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX });
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ TEXCOORD0_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32_SFLOAT, 0 }); // texcoord as vec2
        vertexBindingIndex++;
    }

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

    // if blending is requested setup appropriate colorblendstate
    vsg::ColorBlendState::ColorBlendAttachments colorBlendAttachments;
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    if (shaderModeMask & BLEND)
    {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    colorBlendAttachments.push_back(colorBlendAttachment);

    vsg::GraphicsPipelineStates pipelineStates
    {
        vsg::ShaderStages::create(shaders),
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(colorBlendAttachments),
        vsg::DepthStencilState::create()
    };

    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipeline> graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    return bindGraphicsPipeline;
}
