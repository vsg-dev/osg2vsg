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

#include <osg2vsg/GraphicsNodes.h>

using namespace vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif


/////////////////////////////////////////////////////////////////////////////////////////
//
// GraphicsPipelineGroup
//
GraphicsPipelineGroup::GraphicsPipelineGroup(Allocator* allocator) :
    Inherit(allocator)
{
}

void GraphicsPipelineGroup::read(Input& input)
{
    GraphicsNode::read(input);

    shaders.resize(input.readValue<uint32_t>("NumShader"));
    for (auto& shader : shaders)
    {
        shader = input.readObject<Shader>("Shader");
    }
}

void GraphicsPipelineGroup::write(Output& output) const
{
    GraphicsNode::write(output);

    output.writeValue<uint32_t>("NumShader", shaders.size());
    for (auto& shader : shaders)
    {
        output.writeObject("Shader", shader.get());
    }
}

void GraphicsPipelineGroup::accept(DispatchTraversal& dv) const
{
    if (_bindPipeline) _bindPipeline->accept(dv);
    if (_projPushConstant) _projPushConstant->accept(dv);
    if (_viewPushConstant) _viewPushConstant->accept(dv);

    traverse(dv);

    // do we restore pipelines in parental chain?
}

void GraphicsPipelineGroup::traverse(Visitor& visitor)
{
    if (_bindPipeline) _bindPipeline->accept(visitor);
    if (_projPushConstant) _projPushConstant->accept(visitor);
    if (_viewPushConstant) _viewPushConstant->accept(visitor);

    Inherit::traverse(visitor);
}

void GraphicsPipelineGroup::traverse(ConstVisitor& visitor) const
{
    if (_bindPipeline) _bindPipeline->accept(visitor);
    if (_projPushConstant) _projPushConstant->accept(visitor);
    if (_viewPushConstant) _viewPushConstant->accept(visitor);

    Inherit::traverse(visitor);
}

void GraphicsPipelineGroup::compile(Context& context)
{
    DEBUG_OUTPUT<<"\nGraphicsPipelineGroup::compile(Context& context) "<<this<<std::endl;

    //
    // set up descriptor layout and descriptor set and pipeline layout for uniforms
    //
    context.descriptorPool = DescriptorPool::create(context.device, maxSets, descriptorPoolSizes);
    DEBUG_OUTPUT<<"  context.descriptorPool = "<<context.descriptorPool.get()<<std::endl;

    for (unsigned int i = 0; i < descriptorSetLayoutBindings.size(); i++)
    {
        context.descriptorSetLayouts.push_back(DescriptorSetLayout::create(context.device, descriptorSetLayoutBindings[i]));
        DEBUG_OUTPUT << "  context.descriptorSetLayout = " << context.descriptorSetLayouts[i].get() << std::endl;
    }

    context.pipelineLayout = PipelineLayout::Implementation::create(context.device, context.descriptorSetLayouts, pushConstantRanges);
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
    DEBUG_OUTPUT<<"GraphicsPipelineGroup::compile(Context& context) finished "<<this<<"\n"<<std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// MatrixTransform
//
MatrixTransform::MatrixTransform(Allocator* allocator) :
    Inherit(allocator)
{
    _matrix = new mat4Value;
}

void MatrixTransform::accept(DispatchTraversal& dv) const
{
    if (_pushConstant) _pushConstant->accept(dv);

    traverse(dv);

    // do we restore transforms in parental chain?
}

void MatrixTransform::compile(Context& /*context*/)
{
    _pushConstant = vsg::PushConstants::create(VK_SHADER_STAGE_VERTEX_BIT, 128, _matrix);
}

//
//  Geometry node
//       vertex arrays
//       index arrays
//       draw + draw DrawIndexed
//       push constants for per geometry colours etc.
//
//       Maps to a Group containing StateGroup + Binds + DrawIndex/Draw etc + Push constants
//
/////////////////////////////////////////////////////////////////////////////////////////
//
// Geometry
//
Geometry::Geometry(Allocator* allocator) :
    Inherit(allocator)
{
}

void Geometry::accept(DispatchTraversal& dv) const
{
    if (_renderImplementation) _renderImplementation->accept(dv);
}

void Geometry::compile(Context& context)
{
    auto vertexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, _arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);

    _renderImplementation = new vsg::Group;

    // set up vertex buffer binding
    vsg::ref_ptr<vsg::BindVertexBuffers> bindVertexBuffers = vsg::BindVertexBuffers::create(0, vertexBufferData);  // device dependent
    _renderImplementation->addChild(bindVertexBuffers); // device dependent

    // set up index buffer binding
    if(_indices &&_indices->dataSize() > 0)
    {
        auto indexBufferData = vsg::createBufferAndTransferData(context.device, context.commandPool, context.graphicsQueue, { _indices }, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        vsg::ref_ptr<vsg::BindIndexBuffer> bindIndexBuffer = vsg::BindIndexBuffer::create(indexBufferData.front(), VK_INDEX_TYPE_UINT16); // device dependent
        _renderImplementation->addChild(bindIndexBuffer); // device dependent
    }

    // add the commands in the the _renderImplementation group.
    for(auto& command : _commands) _renderImplementation->addChild(command);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// AttributesNode
//
AttributesNode::AttributesNode(Allocator* allocator) :
    Inherit(allocator)
{

}

void AttributesNode::traverse(Visitor& visitor)
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(visitor);
    Inherit::traverse(visitor);
}

void AttributesNode::traverse(ConstVisitor& visitor) const
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(visitor);
    Inherit::traverse(visitor);
}


void AttributesNode::accept(DispatchTraversal& dv) const
{
    if (_bindDescriptorSets) _bindDescriptorSets->accept(dv);
    traverse(dv);
}


void AttributesNode::compile(Context& context)
{
    // set up DescriptorSet
    vsg::Descriptors attributeDescriptors;

    for(auto attribute : _attributesList)
    {
        attribute->compile(context);
        if(attribute->_descriptor.valid())
        {
            attributeDescriptors.push_back(attribute->_descriptor);
        }
    }

    vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = vsg::DescriptorSet::create(context.device, context.descriptorPool, context.descriptorSetLayouts, attributeDescriptors);

    if (descriptorSet)
    {
        // setup binding of descriptors
        _bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipelineLayout, vsg::DescriptorSets{ descriptorSet }); // device dependent

        DEBUG_OUTPUT << "AttributesNode::compile(() succeeded." << std::endl;
    }
    else
    {
        DEBUG_OUTPUT << "AttributesNode::compile(() failed, descriptorSet not created." << std::endl;
    }
}


void AttributesNode::read(Input& input)
{
    GraphicsNode::read(input);

    _attributesList.resize(input.readValue<uint32_t>("NumAttributes"));
    for (auto& attribute : _attributesList)
    {
        attribute = input.readObject<Attribute>("Attribute");
    }
}

void AttributesNode::write(Output& output) const
{
    GraphicsNode::write(output);

    output.writeValue<uint32_t>("NumAttributes", _attributesList.size());
    for (auto& attribute : _attributesList)
    {
        output.writeObject("Attribute", attribute.get());
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// Attribute
//
Attribute::Attribute(Allocator* allocator) :
    Inherit(allocator)
{

}

void Attribute::read(Input& input)
{
    Object::read(input);

    _bindingIndex = input.readValue<uint32_t>("BindingIndex");
}

void Attribute::write(Output& output) const
{
    Object::write(output);

    output.writeValue<uint32_t>("BindingIndex", _bindingIndex);
}


////////////////////////////////////////////////////////////////////////////////////////
//
// TextureAttribute
//
TextureAttribute::TextureAttribute(Allocator* allocator) :
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
}

void TextureAttribute::compile(Context& context)
{
    ref_ptr<Sampler> sampler = Sampler::create(context.device, _samplerInfo, nullptr);
    vsg::ImageData imageData = vsg::transferImageData(context.device, context.commandPool, context.graphicsQueue, _textureData, sampler);
    if (!imageData.valid())
    {
        DEBUG_OUTPUT << "Texture not created" << std::endl;
        return;
    }

    // set up DescriptorSet
    _descriptor = vsg::DescriptorImage::create(_bindingIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vsg::ImageDataList{imageData});
}
