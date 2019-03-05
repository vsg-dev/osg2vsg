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
