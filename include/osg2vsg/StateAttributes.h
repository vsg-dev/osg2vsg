#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/vk/GraphicsPipeline.h>
#include <vsg/vk/PushConstants.h>
#include <vsg/vk/CommandPool.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/traversals/CompileTraversal.h>

namespace vsg
{
    class DescriptorStateComponent : public Inherit<StateComponent, DescriptorStateComponent>
    {
    public:
        DescriptorStateComponent(Allocator* allocator = nullptr) : Inherit(allocator) {}

        void pushTo(Descriptors& descriptors) const
        {
            if (_descriptor.valid())
            {
                descriptors.push_back(_descriptor);
            }
        }

        // compiled object
        ref_ptr<vsg::Descriptor> _descriptor;
    };
    VSG_type_name(vsg::DescriptorStateComponent);

    class DescriptorSetStateSet : public Inherit<StateSet, DescriptorSetStateSet>
    {
    public:
        DescriptorSetStateSet(Allocator* allocator = nullptr) : Inherit(allocator) {}

        void compile(Context& context)
        {
            Inherit::compile(context);

            Descriptors descriptors;
            for (auto& component : _stateComponents)
            {
                auto descriptorComponent = dynamic_cast<DescriptorStateComponent*>(component.get());
                if(descriptorComponent) descriptorComponent->pushTo(descriptors);
            }
            vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(context.device, context.descriptorPool, context.descriptorSetLayouts[0], descriptors);
            _bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipelineLayout, vsg::DescriptorSets{ descriptorSet });
        }

        void pushTo(State& state) const
        {
            Inherit::pushTo(state);
            _bindDescriptorSets->pushTo(state);
        }

        void popFrom(State& state) const
        {
            Inherit::popFrom(state);
            _bindDescriptorSets->popFrom(state);
        }

        // compild object
        ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;
    };
    VSG_type_name(vsg::DescriptorSetStateSet)

    class GraphicsPipelineAttribute : public Inherit<StateComponent, GraphicsPipelineAttribute>
    {
    public:
        GraphicsPipelineAttribute(Allocator* allocator = nullptr);

        void compile(Context& context) override;

        void pushTo(State& state) const override;
        void popFrom(State& state) const override;

        void dispatch(CommandBuffer& commandBuffer) const override;

        void read(Input& input) override;
        void write(Output& output) const override;


        using Shaders = std::vector<ref_ptr<Shader>>;

        // settings
        // descriptorPool ..
        uint32_t maxSets = 0;
        DescriptorPoolSizes descriptorPoolSizes; // need to accumulate descriptorPoolSizes by looking at scene graph
        // descriptorSetLayout ..
        std::vector<DescriptorSetLayoutBindings> descriptorSetLayoutBindings;
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

    class Texture : public Inherit<DescriptorStateComponent, Texture>
    {
    public:
        Texture(Allocator* allocator = nullptr);

        void compile(Context& context);

        void pushTo(State& state) const override;
        void popFrom(State& state) const override;

        void dispatch(CommandBuffer& commandBuffer) const override;

        // settings
        uint32_t _bindingIndex = 0;
        VkSamplerCreateInfo _samplerInfo;
        ref_ptr<Data> _textureData;

        // compiled objects
        //ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;
    };
    VSG_type_name(vsg::Texture)

}
