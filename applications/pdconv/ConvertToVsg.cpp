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

using namespace osg2vsg;

ConvertToVsg::ConvertToVsg() :
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{
}

vsg::ref_ptr<vsg::BindGraphicsPipeline> ConvertToVsg::getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask)
{
    MaskPair masks(shaderModeMask, geometryMask);
    if (auto itr = pipelineMap.find(masks); itr != pipelineMap.end()) return itr->second;

    auto bindGraphicsPipeline = createBindGraphicsPipeline(shaderModeMask, geometryMask, vertexShaderPath, fragmentShaderPath);
    pipelineMap[masks] = bindGraphicsPipeline;
    return bindGraphicsPipeline;
}

vsg::ref_ptr<vsg::BindDescriptorSet> ConvertToVsg::getOrCreateBindDescriptorSet(uint32_t shaderModeMask, uint32_t geometryMask, osg::StateSet* stateset)
{
    MasksAndState masksAndState(shaderModeMask, geometryMask, stateset);
    if (auto itr = bindDescriptorSetMap.find(masksAndState); itr != bindDescriptorSetMap.end())
    {
        // std::cout<<"reusing bindDescriptorSet "<<itr->second.get()<<std::endl;
        return itr->second;
    }

    MaskPair masks(shaderModeMask, geometryMask);

    auto bindGraphicsPipeline = pipelineMap[masks];
    if (!bindGraphicsPipeline) return {};

    auto pipeline = bindGraphicsPipeline->getPipeline();
    if (!pipeline) return {};

    auto pipelineLayout = pipeline->getPipelineLayout();
    if (!pipelineLayout) return {};

    auto descriptorSet = createVsgStateSet(pipelineLayout->getDescriptorSetLayouts(), stateset, shaderModeMask);
    if (!descriptorSet) return {};

    // std::cout<<"   We have descriptorSet "<<descriptorSet<<std::endl;

    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    bindDescriptorSetMap[masksAndState] = bindDescriptorSet;

    return bindDescriptorSet;
}

void ConvertToVsg::optimize(osg::Node* osg_scene)
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
    optimizer.optimize(osg_scene, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);

    osg2vsg::OptimizeOsgBillboards optimizeBillboards;
    osg_scene->accept(optimizeBillboards);
    optimizeBillboards.optimize();
}

vsg::ref_ptr<vsg::Node> ConvertToVsg::convert(osg::Node* node)
{
    root = nullptr;

    if (auto itr = nodeMap.find(node); itr != nodeMap.end())
    {
        root = itr->second;
    }
    else
    {
        if (node) node->accept(*this);

        nodeMap[node] = root;
    }

    return root;
}

vsg::ref_ptr<vsg::Data> ConvertToVsg::copy(osg::Array* src_array)
{
    if (!src_array) return {};

    switch(src_array->getType())
    {
        case(osg::Array::ByteArrayType): return {};
        case(osg::Array::ShortArrayType): return {};
        case(osg::Array::IntArrayType): return {};

        case(osg::Array::UByteArrayType): return copyArray<vsg::ubyteArray>(src_array);
        case(osg::Array::UShortArrayType): return copyArray<vsg::ushortArray>(src_array);
        case(osg::Array::UIntArrayType): return copyArray<vsg::uintArray>(src_array);

        case(osg::Array::FloatArrayType): return copyArray<vsg::floatArray>(src_array);
        case(osg::Array::DoubleArrayType): return copyArray<vsg::doubleArray>(src_array);

        case(osg::Array::Vec2bArrayType): return {};
        case(osg::Array::Vec3bArrayType): return {};
        case(osg::Array::Vec4bArrayType): return {};

        case(osg::Array::Vec2sArrayType): return {};
        case(osg::Array::Vec3sArrayType): return {};
        case(osg::Array::Vec4sArrayType): return {};

        case(osg::Array::Vec2iArrayType): return {};
        case(osg::Array::Vec3iArrayType): return {};
        case(osg::Array::Vec4iArrayType): return {};

        case(osg::Array::Vec2ubArrayType): return copyArray<vsg::ubvec2Array>(src_array);
        case(osg::Array::Vec3ubArrayType): return copyArray<vsg::ubvec3Array>(src_array);
        case(osg::Array::Vec4ubArrayType): return copyArray<vsg::ubvec4Array>(src_array);

        case(osg::Array::Vec2usArrayType): return copyArray<vsg::usvec2Array>(src_array);
        case(osg::Array::Vec3usArrayType): return copyArray<vsg::usvec3Array>(src_array);
        case(osg::Array::Vec4usArrayType): return copyArray<vsg::usvec4Array>(src_array);

        case(osg::Array::Vec2uiArrayType): return copyArray<vsg::uivec2Array>(src_array);
        case(osg::Array::Vec3uiArrayType): return copyArray<vsg::usvec3Array>(src_array);
        case(osg::Array::Vec4uiArrayType): return copyArray<vsg::usvec4Array>(src_array);

        case(osg::Array::Vec2ArrayType): return copyArray<vsg::vec2Array>(src_array);
        case(osg::Array::Vec3ArrayType): return copyArray<vsg::vec3Array>(src_array);
        case(osg::Array::Vec4ArrayType): return copyArray<vsg::vec4Array>(src_array);

        case(osg::Array::Vec2dArrayType): return copyArray<vsg::dvec2Array>(src_array);
        case(osg::Array::Vec3dArrayType): return copyArray<vsg::dvec2Array>(src_array);
        case(osg::Array::Vec4dArrayType): return copyArray<vsg::dvec2Array>(src_array);

        case(osg::Array::MatrixArrayType): return copyArray<vsg::mat4Array>(src_array);
        case(osg::Array::MatrixdArrayType): return copyArray<vsg::dmat4Array>(src_array);

        case(osg::Array::QuatArrayType): return {};

        case(osg::Array::UInt64ArrayType): return {};
        case(osg::Array::Int64ArrayType): return {};
        default: return {};
    }
}

uint32_t ConvertToVsg::calculateShaderModeMask()
{
    if (statestack.empty()) return osg2vsg::ShaderModeMask::NONE;

    auto& statepair = getStatePair();

    return osg2vsg::calculateShaderModeMask(statepair.first) | osg2vsg::calculateShaderModeMask(statepair.second);
}

void ConvertToVsg::apply(osg::Geometry& geometry)
{
    ScopedPushPop spp(*this, geometry.getStateSet());

    uint32_t geometryMask = (osg2vsg::calculateAttributesMask(&geometry) | overrideGeomAttributes) & supportedGeometryAttributes;
    uint32_t shaderModeMask = (calculateShaderModeMask() | overrideShaderModeMask) & supportedShaderModeMask;

    // std::cout<<"Have geometry with "<<statestack.size()<<" shaderModeMask="<<shaderModeMask<<", geometryMask="<<geometryMask<<std::endl;

    auto stategroup = vsg::StateGroup::create();

    auto bindGraphicsPipeline = getOrCreateBindGraphicsPipeline(shaderModeMask, geometryMask);
    if (bindGraphicsPipeline) stategroup->add(bindGraphicsPipeline);

    auto vsg_geometry = osg2vsg::convertToVsg(&geometry, geometryMask, geometryTarget);

    if (!statestack.empty())
    {
        auto stateset = getStatePair().second;
        //std::cout<<"   We have stateset "<<stateset<<", descriptorSetLayouts.size() = "<<descriptorSetLayouts.size()<<", "<<shaderModeMask<<std::endl;
        if (stateset)
        {
            auto bindDescriptorSet = getOrCreateBindDescriptorSet(shaderModeMask, geometryMask, stateset);
            if (bindDescriptorSet) stategroup->add(bindDescriptorSet);
        }
    }

    stategroup->addChild(vsg_geometry);

    root = stategroup;
}

void ConvertToVsg::apply(osg::Group& group)
{
    auto vsg_group = vsg::Group::create();

    ScopedPushPop spp(*this, group.getStateSet());

    //vsg_group->setValue("class", group.className());

    for(unsigned int i=0; i<group.getNumChildren(); ++i)
    {
        auto child = group.getChild(i);
        if (auto vsg_child = convert(child); vsg_child)
        {
            vsg_group->addChild(vsg_child);
        }
    }

    root = vsg_group;
}

void ConvertToVsg::apply(osg::MatrixTransform& transform)
{
    osg::Matrix matrix = transform.getMatrix();

    vsg::mat4 vsg_matrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                        matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                        matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                        matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));

    auto vsg_transform = vsg::MatrixTransform::create();
    vsg_transform->setMatrix(vsg_matrix);

    for(unsigned int i=0; i<transform.getNumChildren(); ++i)
    {
        auto child = transform.getChild(i);
        if (auto vsg_child = convert(child); vsg_child)
        {
            vsg_transform->addChild(vsg_child);
        }
    }

    root = vsg_transform;
}

#if 0
void apply(osg::LOD& lod)
{
    traverse(plod);
}

void apply(osg::CoordinateSystemNode& cs)
{
    traverse(plod);
}

void apply(osg::PagedLOD& plod)
{
    auto vsg_plod = vsg::PagedLOD::create();

    auto& vsg_plod_children  = vsg_plod->getChildren();

    addToChildrenStackAndPushNewChildrenStack(vsg_plod);

    for(unsigned int i=0; i<plod.getNumFileNames(); ++i)
    {
        auto& vsg_child = vsg_plod_children[i];

        if (plod.getChild(i))
        {
            Objects& vsg_nested_children = childrenStack.back();
            vsg_nested_children.clear();

            plod.getChild(i)->accept(*this);

            if (!vsg_nested_children.empty())
            {
                Objects& vsg_nested_children = childrenStack.back();

                vsg_child.child = dynamic_cast<vsg::Node*>(vsg_nested_children[0].get());

                vsg_nested_children.clear();
            }
        }

        if (!plod.getFileName(i).empty())
        {
            vsg_child.filename = plod.getFileName(i);
            filenames.push_back(vsg_child.filename);
        }
    }

    popChildrenStack();
}


void apply(osg::Node& node)
{
    auto vsg_cs = vsg::Group::create();
    vsg_cs->setValue("class", node.className());

    addToChildrenStackAndPushNewChildrenStack(vsg_cs);

    // traveer the nodes children
    traverse(node);

    // handle any VSG children created by traverse
    Objects& children = childrenStack.back();

    std::cout<<"number of children = "<<children.size()<<std::endl;

    for(auto& child : children)
    {
        vsg::ref_ptr<vsg::Node> child_node(dynamic_cast<vsg::Node*>(child.get()));
        if (child_node) vsg_cs->addChild(child_node);
        else
        {
            std::cout<<"Child is not a node, cannot add to group : "<<child->className()<<std::endl;
        }
    }

    popChildrenStack();
}
#endif
