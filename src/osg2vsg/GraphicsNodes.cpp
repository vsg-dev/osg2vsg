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

