#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/vk/PushConstants.h>
#include <vsg/vk/CommandPool.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/traversals/CompileTraversal.h>

#include <map>

namespace vsg
{
    using Shaders = std::vector<ref_ptr<Shader>>;

    class GraphicsPipelineAttribute : public Inherit<StateCommand, GraphicsPipelineAttribute>
    {
    public:
        GraphicsPipelineAttribute(Allocator* allocator = nullptr);

        void compile(Context& context) override;

        void pushTo(State& state) const override;
        void popFrom(State& state) const override;

        void dispatch(CommandBuffer& commandBuffer) const override;

        void read(Input& input) override;
        void write(Output& output) const override;


        // settings
        // descriptorPool ..
        uint32_t maxSets = 0;
        DescriptorPoolSizes descriptorPoolSizes; // need to accumulate descriptorPoolSizes by looking at scene graph

        ref_ptr<PipelineLayout> pipelineLayout;
        vsg::DescriptorSetLayouts descriptorSetLayouts;

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
    VSG_type_name(vsg::GraphicsPipelineAttribute)

    class Texture : public Inherit<Descriptor, Texture>
    {
    public:
        Texture();

        void compile(Context& context) override;

        void assignTo(VkWriteDescriptorSet& wds, VkDescriptorSet descriptorSet) const override;

        // settings
        VkSamplerCreateInfo _samplerInfo;
        ref_ptr<Data> _textureData;

        ref_ptr<vsg::Descriptor> _implementation;
    };
    VSG_type_name(vsg::Texture)

}
