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

    class VSG_DECLSPEC SharedBindDescriptorSets : public Inherit<BindDescriptorSets, SharedBindDescriptorSets>
    {
    public:
        //SharedBindDescriptorSets() {}

        SharedBindDescriptorSets(VkPipelineBindPoint bindPoint = {}, PipelineLayout* pipelineLayout = nullptr, const DescriptorSets& descriptorSets = {}) :
            Inherit(bindPoint, pipelineLayout, descriptorSets)
        {
        }

        void addDescriptor(vsg::ref_ptr<Descriptor> descriptor, const uint32_t& setIndex = 0)
        {
            _descriptorSetBindingsMap[setIndex].push_back(descriptor);
        }

        void compile(Context& context)
        {
            _bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            _pipelineLayout = context.pipelineLayout;

            _descriptorSets.clear();
            for (auto& setDescriptorsPair : _descriptorSetBindingsMap)
            {
                vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(context.device, context.descriptorPool, context.descriptorSetLayouts[setDescriptorsPair.first], setDescriptorsPair.second);
                _descriptorSets.push_back(descriptorSet);
            }
            update();
        }

    protected:
        virtual ~SharedBindDescriptorSets() {}

        using DescriptorSetBindingsMap = std::map<uint32_t, Descriptors>;
        DescriptorSetBindingsMap _descriptorSetBindingsMap;
    };
    VSG_type_name(vsg::SharedBindDescriptorSets);

    class Texture : public Inherit<StateComponent, Texture>
    {
    public:
        Texture(ref_ptr<SharedBindDescriptorSets> sharedBindDescriptorSets = ref_ptr<SharedBindDescriptorSets>(), Allocator* allocator = nullptr);

        void compile(Context& context);

        void pushTo(State& state) const override;
        void popFrom(State& state) const override;

        void dispatch(CommandBuffer& commandBuffer) const override;

        // settings
        uint32_t _bindingIndex = 0;
        VkSamplerCreateInfo _samplerInfo;
        ref_ptr<Data> _textureData;

        // compiled objects
        ref_ptr<vsg::BindDescriptorSets> _bindDescriptorSets;

        // for demo purpose
        bool _ownsBindDescriptorSets = true;
    };
    VSG_type_name(vsg::Texture)

}
