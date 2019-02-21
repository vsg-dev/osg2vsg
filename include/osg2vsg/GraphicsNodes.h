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

    class GraphicsPipelineGroup : public Inherit<GraphicsNode, GraphicsPipelineGroup>
    {
    public:
        GraphicsPipelineGroup(Allocator* allocator = nullptr);

        using Inherit::traverse;

        void traverse(Visitor& visitor) override;
        void traverse(ConstVisitor& visitor) const override;

        void accept(DispatchTraversal& dv) const override;

        void compile(Context& context) override;

        void read(Input& input) override;
        void write(Output& output) const override;


        using Shaders = std::vector<ref_ptr<Shader>>;

        // settings
        // descriptorPool ..
        uint32_t maxSets = 0;
        DescriptorPoolSizes descriptorPoolSizes; // need to accumulate descriptorPoolSizes by looking at scene graph
        // descriptorSetLayout ..
        DescriptorSetLayoutBindings descriptorSetLayoutBindings;
        PushConstantRanges pushConstantRanges;
        VertexInputState::Bindings vertexBindingsDescriptions;
        VertexInputState::Attributes vertexAttributeDescriptions;
        Shaders shaders;
        GraphicsPipelineStates pipelineStates;

        // compiled objects
        ref_ptr<BindPipeline> _bindPipeline;
        ref_ptr<PushConstants> _projPushConstant;
        ref_ptr<PushConstants> _viewPushConstant;
    };
    VSG_type_name(vsg::GraphicsPipelineGroup)

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

    class Attribute : public Inherit<Object, Attribute>
    {
    public:
        Attribute(Allocator* allocator = nullptr);

        void read(Input& input) override;
        void write(Output& output) const override;

        virtual void compile(Context& context) = 0;

        // settings
        uint32_t _bindingIndex = 0;

        // compiled object
        ref_ptr<vsg::Descriptor> _descriptor;
    };
    VSG_type_name(vsg::Attribute)

    class TextureAttribute : public Inherit<Attribute, TextureAttribute>
    {
    public:
        TextureAttribute(Allocator* allocator = nullptr);

        void compile(Context& context) override;

        // settings
        VkSamplerCreateInfo _samplerInfo;
        ref_ptr<Data> _textureData;
    };
    VSG_type_name(vsg::TextureAttribute)

    class AttributesNode : public Inherit<GraphicsNode, AttributesNode>
    {
    public:
        AttributesNode(Allocator* allocator = nullptr);

        using Inherit::traverse;

        void traverse(Visitor& visitor) override;
        void traverse(ConstVisitor& visitor) const override;

        void accept(DispatchTraversal& dv) const override;

        void read(Input& input) override;
        void write(Output& output) const override;

        void compile(Context& context) override;

        using AttributesList = std::vector<ref_ptr<Attribute>>;

        // settings
        AttributesList _attributesList;

        // compiled objects
        ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;
    };
    VSG_type_name(vsg::AttributesNode)
}
