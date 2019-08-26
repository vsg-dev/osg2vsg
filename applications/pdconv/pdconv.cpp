#include <vsg/all.h>

#include <osg/ArgumentParser>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>

#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/SceneBuilder.h>


class ConvertToVsg : public osg::NodeVisitor, public osg2vsg::SceneBuilderBase
{
public:

    ConvertToVsg() :
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    vsg::ref_ptr<vsg::Node> root;

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

        auto& statepair = stateMap[statestack];
        if (!statepair.first && !statepair.second)
        {
            osg::ref_ptr<osg::StateSet> combined;
            if (statestack.size()==1)
            {
                std::cout<<"Assigning lone stateset"<<std::endl;
                combined = statestack.back();
            }
            else
            {
                std::cout<<"Assigning combined stateset"<<std::endl;
                combined = new osg::StateSet;
                for(auto& stateset : statestack)
                {
                    combined->merge(*stateset);
                }
            }

        }
        else
        {
                std::cout<<"Reusing exisitng"<<std::endl;
        }

        return osg2vsg::calculateShaderModeMask(statepair.first);
    }

    void apply(osg::Geometry& geometry)
    {
        ScopedPushPop spp(*this, geometry.getStateSet());

        uint32_t geometryMask = (osg2vsg::calculateAttributesMask(&geometry) | overrideGeomAttributes) & supportedGeometryAttributes;
        uint32_t shaderModeMask = (calculateShaderModeMask() | overrideShaderModeMask) & supportedShaderModeMask;

        std::cout<<"Have geometry with "<<statestack.size()<<" shaderModeMask="<<shaderModeMask<<", geometryMask="<<geometryMask<<std::endl;

        uint32_t requiredAttributesMask = 0;

        root = osg2vsg::convertToVsg(&geometry, requiredAttributesMask, geometryTarget);
    }

    void apply(osg::Group& group)
    {
        auto vsg_group = vsg::Group::create();

        ScopedPushPop spp(*this, group.getStateSet());

        vsg_group->setValue("class", group.className());

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
    vsg::CommandLine arguments(&argc, argv);
    auto levels = arguments.value(3, "-l");
    auto outputFilename = arguments.value(std::string(), "-o");

    std::cout<<"levels = "<<levels<<std::endl;
    std::cout<<"outputFilename = "<<outputFilename<<std::endl;

    osg::ArgumentParser osg_arguments(&argc, argv);

    osg::ref_ptr<osg::Node> osg_scene = osgDB::readNodeFiles(osg_arguments);

    std::cout<<"osg_scene = "<<osg_scene.get()<<std::endl;

    if (osg_scene.valid())
    {
        ConvertToVsg convertToVsg;
        osg_scene->accept(convertToVsg);

        if (convertToVsg.root && !outputFilename.empty())
        {
            vsg::write(convertToVsg.root, outputFilename);
        }
    }


    return 1;
}
