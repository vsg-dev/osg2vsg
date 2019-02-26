#include <vsg/vk/BufferData.h>
#include <vsg/vk/Descriptor.h>
#include <vsg/vk/DescriptorSet.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/vk/BindVertexBuffers.h>
#include <vsg/vk/BindIndexBuffer.h>
#include <vsg/vk/Draw.h>

#include <vsg/io/ReaderWriter.h>

#include <vsg/traversals/DispatchTraversal.h>

#include <iostream>

#include <osg2vsg/StateAttributes.h>

using namespace vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif


/////////////////////////////////////////////////////////////////////////////////////////
//
// GraphicsPipelineAttribute
//
GraphicsPipelineAttribute::GraphicsPipelineAttribute(Allocator* allocator) :
    Inherit(allocator)
{
}

void GraphicsPipelineAttribute::pushTo(State& state) const
{
    if (_bindPipeline) _bindPipeline->pushTo(state);
    if (_projPushConstant) _projPushConstant->pushTo(state);
    if (_viewPushConstant) _viewPushConstant->pushTo(state);
}

void GraphicsPipelineAttribute::popFrom(State& state) const
{
    if (_bindPipeline) _bindPipeline->popFrom(state);
    if (_projPushConstant) _projPushConstant->popFrom(state);
    if (_viewPushConstant) _viewPushConstant->popFrom(state);
}

void GraphicsPipelineAttribute::dispatch(CommandBuffer& commandBuffer) const
{
    if (_bindPipeline) _bindPipeline->dispatch(commandBuffer);
    if (_projPushConstant) _projPushConstant->dispatch(commandBuffer);
    if (_viewPushConstant) _viewPushConstant->dispatch(commandBuffer);
}

void GraphicsPipelineAttribute::read(Input& input)
{
    StateComponent::read(input);

    shaders.resize(input.readValue<uint32_t>("NumShader"));
    for (auto& shader : shaders)
    {
        shader = input.readObject<Shader>("Shader");
    }
}

void GraphicsPipelineAttribute::write(Output& output) const
{
    StateComponent::write(output);

    output.writeValue<uint32_t>("NumShader", shaders.size());
    for (auto& shader : shaders)
    {
        output.writeObject("Shader", shader.get());
    }
}

void GraphicsPipelineAttribute::compile(Context& context)
{
    DEBUG_OUTPUT<<"\nGraphicsPipelineAttribute::compile(Context& context) "<<this<<std::endl;

    //
    // set up descriptor layout and descriptor set and pipeline layout for uniforms
    //
    if (!descriptorPoolSizes.empty()) context.descriptorPool = DescriptorPool::create(context.device, maxSets, descriptorPoolSizes);
    DEBUG_OUTPUT<<"  context.descriptorPool = "<<context.descriptorPool.get()<<std::endl;

    // prevent previous GraphicsPipelineAttribute::compile(Context& context) calls during the current compile traversal accumulating into this GraphicsPipeline setup.
    context.descriptorSetLayouts.clear();

    for (unsigned int i = 0; i < descriptorSetLayoutBindings.size(); i++)
    {
        context.descriptorSetLayouts.push_back(DescriptorSetLayout::create(context.device, descriptorSetLayoutBindings[i]));
        DEBUG_OUTPUT << "  context.descriptorSetLayout = " << context.descriptorSetLayouts[i].get() << std::endl;
    }

    context.pipelineLayout = PipelineLayout::create(context.device, context.descriptorSetLayouts, pushConstantRanges);
    DEBUG_OUTPUT<<"  context.pipelineLayout = "<<context.pipelineLayout.get()<<std::endl;


    ShaderModules shaderModules;
    shaderModules.reserve(shaders.size());
    for(auto& shader : shaders)
    {
        shaderModules.emplace_back(ShaderModule::create(context.device, shader));
    }

    auto shaderStages = ShaderStages::create(shaderModules);

    DEBUG_OUTPUT<<"  shaderStages = "<<shaderStages.get()<<std::endl;

    GraphicsPipelineStates full_pipelineStates = pipelineStates;
    full_pipelineStates.emplace_back(context.viewport);
    full_pipelineStates.emplace_back(shaderStages);
    full_pipelineStates.emplace_back(VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions));


    ref_ptr<GraphicsPipeline> pipeline = GraphicsPipeline::create(context.device, context.renderPass, context.pipelineLayout, full_pipelineStates);

    DEBUG_OUTPUT<<"  pipeline = "<<pipeline.get()<<std::endl;

    _bindPipeline = BindPipeline::create(pipeline);

    DEBUG_OUTPUT<<"  _bindPipeline = "<<_bindPipeline.get()<<std::endl;

    if (context.projMatrix) _projPushConstant = PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 0, context.projMatrix);
    if (context.viewMatrix) _viewPushConstant = PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 64, context.viewMatrix);
    DEBUG_OUTPUT<<"GraphicsPipelineAttribute::compile(Context& context) finished "<<this<<"\n"<<std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// Texture
//
Texture::Texture(ref_ptr<SharedBindDescriptorSets> sharedBindDescriptorSets, Allocator* allocator) :
    Inherit(allocator)
{
    // set default sampler info
    _samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    _samplerInfo.minFilter = VK_FILTER_LINEAR;
    _samplerInfo.magFilter = VK_FILTER_LINEAR;
    _samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    _samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
#if 1
    // requres Logical device to have deviceFeatures.samplerAnisotropy = VK_TRUE; set when creating the vsg::Device
    _samplerInfo.anisotropyEnable = VK_TRUE;
    _samplerInfo.maxAnisotropy = 16;
#else
    _samplerInfo.anisotropyEnable = VK_FALSE;
    _samplerInfo.maxAnisotropy = 1;
#endif
    _samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    _samplerInfo.unnormalizedCoordinates = VK_FALSE;
    _samplerInfo.compareEnable = VK_FALSE;
    _samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (sharedBindDescriptorSets.valid())
    {
        _bindDescriptorSets = sharedBindDescriptorSets;
        _ownsBindDescriptorSets = false;
    }
}

void Texture::compile(Context& context)
{
    ref_ptr<Sampler> sampler = Sampler::create(context.device, _samplerInfo, nullptr);
    vsg::ImageData imageData = vsg::transferImageData(context.device, context.commandPool, context.graphicsQueue, _textureData, sampler);
    if (!imageData.valid())
    {
        DEBUG_OUTPUT<<"Texture not created"<<std::endl;
        return;
    }

    if(_ownsBindDescriptorSets)
    {
        // set up DescriptorSet
        vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(context.device, context.descriptorPool, context.descriptorSetLayouts[0],
        {
            vsg::DescriptorImage::create(_bindingIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData})
        });

        if (descriptorSet)
        {
            // setup binding of descriptors
            _bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipelineLayout, vsg::DescriptorSets{descriptorSet}); // device dependent

            DEBUG_OUTPUT<<"Texture::compile(() succeeded."<<std::endl;
            DEBUG_OUTPUT<<"   imageData._sampler = "<<imageData._sampler.get()<<std::endl;
            DEBUG_OUTPUT<<"   imageData._imageView = "<<imageData._imageView.get()<<std::endl;
            DEBUG_OUTPUT<<"   imageData._imageLayout = "<<imageData._imageLayout<<std::endl;
        }
        else
        {
            DEBUG_OUTPUT<<"Texture::compile(() failed, descriptorSet not created."<<std::endl;
            DEBUG_OUTPUT<<"   imageData._sampler = "<<imageData._sampler.get()<<std::endl;
            DEBUG_OUTPUT<<"   imageData._imageView = "<<imageData._imageView.get()<<std::endl;
            DEBUG_OUTPUT<<"   imageData._imageLayout = "<<imageData._imageLayout<<std::endl;
            DEBUG_OUTPUT<<"   _textureData = "<<_textureData->width()<<", "<<_textureData->height()<<", "<<_textureData->depth()<<", "<<std::endl;
        }
    }
    else
    {
        SharedBindDescriptorSets* asShared = dynamic_cast<SharedBindDescriptorSets*>(_bindDescriptorSets.get());
        asShared->addDescriptor(vsg::DescriptorImage::create(_bindingIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{ imageData }), 0);
    }
}

void Texture::pushTo(State& state) const
{
    if (_bindDescriptorSets && _ownsBindDescriptorSets) _bindDescriptorSets->pushTo(state);
}

void Texture::popFrom(State& state) const
{
    if (_bindDescriptorSets && _ownsBindDescriptorSets) _bindDescriptorSets->popFrom(state);
}

void Texture::dispatch(CommandBuffer& commandBuffer) const
{
    if (_bindDescriptorSets && _ownsBindDescriptorSets) _bindDescriptorSets->dispatch(commandBuffer);
}
