#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/vk/PushConstants.h>
#include <vsg/vk/CommandPool.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/traversals/CompileTraversal.h>

namespace vsg
{

    class MatrixTransform : public Inherit<GraphicsNode, MatrixTransform>
    {
    public:
        MatrixTransform(Allocator* allocator = nullptr);

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        // settings
        ref_ptr<mat4Value> _matrix;

        // compiled objects
        ref_ptr<PushConstants> _pushConstant;
    };
    VSG_type_name(vsg::MatrixTransform)

    class Geometry : public Inherit<GraphicsNode, Geometry>
    {
    public:
        Geometry(Allocator* allocator = nullptr);

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        using Commands = std::vector<ref_ptr<Command>>;

        // settings
        DataList _arrays;
        ref_ptr<Data> _indices;
        Commands _commands;

        // compiled objects
        ref_ptr<Group> _renderImplementation;
    };
    VSG_type_name(vsg::Geometry)

}
