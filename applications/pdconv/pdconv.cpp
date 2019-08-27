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


class ConvertToVsg : public osg::NodeVisitor, public osg2vsg::SceneBuilderBase
{
public:

    ConvertToVsg() :
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    vsg::ref_ptr<vsg::Node> root;

    using MaskPair = std::pair<uint32_t, uint32_t>;
    using PipelineMap = std::map<MaskPair, vsg::ref_ptr<vsg::BindGraphicsPipeline>>;
    PipelineMap pipelineMap;

    using MasksAndState = std::tuple<uint32_t, uint32_t, osg::ref_ptr<osg::StateSet>>;
    using BindDescriptorSetMap = std::map<MasksAndState, vsg::ref_ptr<vsg::BindDescriptorSet>>;
    BindDescriptorSetMap bindDescriptorSetMap;

    vsg::ref_ptr<vsg::BindGraphicsPipeline> getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask)
    {
        MaskPair masks(shaderModeMask, geometryMask);
        if (auto itr = pipelineMap.find(masks); itr != pipelineMap.end()) return itr->second;

        auto bindGraphicsPipeline = createBindGraphicsPipeline(shaderModeMask, geometryMask, vertexShaderPath, fragmentShaderPath);
        pipelineMap[masks] = bindGraphicsPipeline;
        return bindGraphicsPipeline;
    }

    vsg::ref_ptr<vsg::BindDescriptorSet> getOrCreateBindDescriptorSet(uint32_t shaderModeMask, uint32_t geometryMask, osg::StateSet* stateset)
    {
        MasksAndState masksAndState(shaderModeMask, geometryMask, stateset);
        if (auto itr = bindDescriptorSetMap.find(masksAndState); itr != bindDescriptorSetMap.end())
        {
            std::cout<<"reusing bindDescriptorSet "<<itr->second.get()<<std::endl;
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

        std::cout<<"   We have descriptorSet "<<descriptorSet<<std::endl;

        auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

        bindDescriptorSetMap[masksAndState] = bindDescriptorSet;

        return bindDescriptorSet;
    }

    void optimize(osg::Node* osg_scene)
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

    vsg::ref_ptr<vsg::Node> convert(osg::Node* node)
    {
        root = nullptr;
        node->accept(*this);
        return root;
    }

    template<class V>
    vsg::ref_ptr<V> copyArray(const osg::Array* array)
    {
        vsg::ref_ptr<V> new_array = V::create( array->getNumElements() );

        std::memcpy(new_array->dataPointer(), array->getDataPointer(), array->getTotalDataSize());

        return new_array;
    }

    vsg::ref_ptr<vsg::Data> copy(osg::Array* src_array)
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

    struct ScopedPushPop
    {
        ConvertToVsg& convertToVsg;
        osg::StateSet* stateset;

        ScopedPushPop(ConvertToVsg& conv, osg::StateSet* ss) :
            convertToVsg(conv),
            stateset(ss)
        {
            if (stateset) convertToVsg.statestack.push_back(stateset);
        }

        ~ScopedPushPop()
        {
            if (stateset) convertToVsg.statestack.pop_back();
        }
    };

    uint32_t calculateShaderModeMask()
    {
        if (statestack.empty()) return osg2vsg::ShaderModeMask::NONE;

        auto& statepair = getStatePair();

        return osg2vsg::calculateShaderModeMask(statepair.first) | osg2vsg::calculateShaderModeMask(statepair.second);
    }

    void apply(osg::Geometry& geometry)
    {
        ScopedPushPop spp(*this, geometry.getStateSet());

        uint32_t geometryMask = (osg2vsg::calculateAttributesMask(&geometry) | overrideGeomAttributes) & supportedGeometryAttributes;
        uint32_t shaderModeMask = (calculateShaderModeMask() | overrideShaderModeMask) & supportedShaderModeMask;

        std::cout<<"Have geometry with "<<statestack.size()<<" shaderModeMask="<<shaderModeMask<<", geometryMask="<<geometryMask<<std::endl;

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

    void apply(osg::Group& group)
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

    void addToChildrenStackAndPushNewChildrenStack(vsg::ref_ptr<vsg::Object> object)
    {
        if (childrenStack.empty()) childrenStack.emplace_back(Objects());
        if (!root) root = object;

        childrenStack.back().push_back(object);

        childrenStack.emplace_back(Objects());
    }

    void popChildrenStack()
    {
        childrenStack.pop_back();
    }

    vsg::ref_ptr<vsg::Object> root;
    using Objects = std::vector<vsg::ref_ptr<vsg::Object>>;
    using ChildrenStack = std::list<Objects>;
    ChildrenStack childrenStack;
    Objects nodeStack;

    vsg::Paths filenames;
#endif
};

int main(int argc, char** argv)
{
    ConvertToVsg sceneBuilder;

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
