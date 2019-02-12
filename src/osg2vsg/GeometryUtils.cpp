#include <osg2vsg/GeometryUtils.h>

#include <osg2vsg/ShaderUtils.h>
#include <osgUtil/MeshOptimizers>

namespace osg2vsg
{

    vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray, bool duplicate, uint32_t dupcount)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec2Array>();

        uint32_t count = duplicate ? dupcount : inarray->size();
        vsg::ref_ptr<vsg::vec2Array> outarray(new vsg::vec2Array(count));
        for (unsigned int i = 0; i < count; i++)
        {
            const osg::Vec2& osg2 = inarray->at(duplicate ? 0 : i);
            vsg::vec2 vsg2(osg2.x(), osg2.y());
            outarray->set(i, vsg2);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray, bool duplicate, uint32_t dupcount)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec3Array>();

        uint32_t count = duplicate ? dupcount : inarray->size();
        vsg::ref_ptr<vsg::vec3Array> outarray(new vsg::vec3Array(count));
        for (unsigned int i = 0; i < count; i++)
        {
            const osg::Vec3& osg3 = inarray->at(duplicate ? 0 : i);
            vsg::vec3 vsg3(osg3.x(), osg3.y(), osg3.z());
            outarray->set(i, vsg3);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray, bool duplicate, uint32_t dupcount)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec4Array>();

        uint32_t count = duplicate ? dupcount : inarray->size();
        vsg::ref_ptr<vsg::vec4Array> outarray(new vsg::vec4Array(count));
        for (unsigned int i = 0; i < count; i++)
        {
            const osg::Vec4& osg4 = inarray->at(duplicate ? 0 : i);
            vsg::vec4 vsg4(osg4.x(), osg4.y(), osg4.z(), osg4.w());
            outarray->set(i, vsg4);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray, bool duplicate, uint32_t dupcount)
    {
        if (!inarray) return vsg::ref_ptr<vsg::Data>();

        switch (inarray->getType())
        {
            case osg::Array::Type::Vec2ArrayType: return convertToVsg(dynamic_cast<const osg::Vec2Array*>(inarray), duplicate, dupcount);
            case osg::Array::Type::Vec3ArrayType: return convertToVsg(dynamic_cast<const osg::Vec3Array*>(inarray), duplicate, dupcount);
            case osg::Array::Type::Vec4ArrayType: return convertToVsg(dynamic_cast<const osg::Vec4Array*>(inarray), duplicate, dupcount);
            default: return vsg::ref_ptr<vsg::Data>();
        }
    }

    uint32_t calculateAttributesMask(const osg::Geometry* geometry)
    {
        uint32_t mask = 0;
        if (geometry->getVertexArray() != nullptr) mask |= VERTEX;
        if (geometry->getNormalArray() != nullptr) mask |= NORMAL;
        if (geometry->getColorArray() != nullptr) mask |= COLOR;
        if (geometry->getTexCoordArray(0) != nullptr) mask |= TEXCOORD0;
        return mask;
    }

    VkPrimitiveTopology convertToTopology(osg::PrimitiveSet::Mode primitiveMode)
    {
        switch (primitiveMode)
        {
            case osg::PrimitiveSet::Mode::POINTS: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case osg::PrimitiveSet::Mode::LINES: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case osg::PrimitiveSet::Mode::LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case osg::PrimitiveSet::Mode::TRIANGLES: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case osg::PrimitiveSet::Mode::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case osg::PrimitiveSet::Mode::TRIANGLE_FAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            case osg::PrimitiveSet::Mode::LINES_ADJACENCY: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
            case osg::PrimitiveSet::Mode::LINE_STRIP_ADJACENCY: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
            case osg::PrimitiveSet::Mode::TRIANGLES_ADJACENCY: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
            case osg::PrimitiveSet::Mode::TRIANGLE_STRIP_ADJACENCY: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
            case osg::PrimitiveSet::Mode::PATCHES: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

            //not supported
            case osg::PrimitiveSet::Mode::LINE_LOOP:
            case osg::PrimitiveSet::Mode::QUADS:
            case osg::PrimitiveSet::Mode::QUAD_STRIP:
            case osg::PrimitiveSet::Mode::POLYGON:
            default: return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM; // use as unsupported flag`
        }
    }

    vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* ingeometry, uint32_t requiredAttributesMask)
    {
        bool hasrequirements = requiredAttributesMask != 0;

        // convert attribute arrays, create defaults for any requested that don't exist for now to ensure pipline gets required data
        vsg::ref_ptr<vsg::Data> vertices(osg2vsg::convertToVsg(ingeometry->getVertexArray()));
        if (!vertices.valid() || vertices->valueCount() == 0) return vsg::ref_ptr<vsg::Geometry>();

        unsigned int vertcount = vertices->valueCount();

        vsg::ref_ptr<vsg::Data> normals(osg2vsg::convertToVsg(ingeometry->getNormalArray(), ingeometry->getNormalBinding() == osg::Array::Binding::BIND_OVERALL, vertcount));
        if ((!normals.valid() || normals->valueCount() == 0) && (requiredAttributesMask & NORMAL)) // if no normals but we've requested them, add them
        {
            vsg::ref_ptr<vsg::vec3Array> defaultnormals(new vsg::vec3Array(vertcount));
            for (unsigned int i = 0; i < vertcount; i++) defaultnormals->set(i, vsg::vec3(0.0f,1.0f,0.0f));
            normals = defaultnormals;
        }

        vsg::ref_ptr<vsg::Data> colors(osg2vsg::convertToVsg(ingeometry->getColorArray(), ingeometry->getColorBinding() == osg::Array::Binding::BIND_OVERALL, vertcount));
        if ((!colors.valid() || colors->valueCount() == 0) && (requiredAttributesMask & COLOR)) // if no colors but we've requested them, add them
        {
            vsg::ref_ptr<vsg::vec3Array> defaultcolors(new vsg::vec3Array(vertcount));
            for (unsigned int i = 0; i < vertcount; i++) defaultcolors->set(i, vsg::vec3(1.0f, 1.0f, 1.0f));
            colors = defaultcolors;
        }

        vsg::ref_ptr<vsg::Data> texcoord0(osg2vsg::convertToVsg(ingeometry->getTexCoordArray(0)));
        if ((!texcoord0.valid() || texcoord0->valueCount() == 0) && (requiredAttributesMask & TEXCOORD0)) // if no texcoord but we've requested them, add them
        {
            vsg::ref_ptr<vsg::vec2Array> defaulttex0(new vsg::vec2Array(vertcount));
            for (unsigned int i = 0; i < vertcount; i++) defaulttex0->set(i, vsg::vec2(0.0f, 0.0f));
            texcoord0 = defaulttex0;
        }

        // fill arrays data list
        auto attributeArrays = vsg::DataList{ vertices }; // always have verticies
        if ((normals.valid() && normals->valueCount() > 0) && (!hasrequirements || (requiredAttributesMask & NORMAL))) attributeArrays.push_back(normals);
        if ((colors.valid() && colors->valueCount() > 0) && (!hasrequirements || (requiredAttributesMask & COLOR))) attributeArrays.push_back(colors);
        if ((texcoord0.valid() && texcoord0->valueCount() > 0) && (!hasrequirements || (requiredAttributesMask & TEXCOORD0))) attributeArrays.push_back(texcoord0);

        // convert indicies
        osg::Geometry::DrawElementsList drawElementsList;
        ingeometry->getDrawElementsList(drawElementsList);

        // asume all the draw elements use the same primitive mode

        std::vector<uint16_t> indcies;
        for(auto drawel : drawElementsList)
        {
            auto numindcies = drawel->getNumIndices();
            for (unsigned int i = 0; i < numindcies; i++)
            {
                indcies.push_back(drawel->index(i));
            }
        }

        vsg::ref_ptr<vsg::ushortArray> vsgindices(new vsg::ushortArray(indcies.size()));
        std::copy(indcies.begin(), indcies.end(), reinterpret_cast<uint16_t*>(vsgindices->dataPointer()));

        // create the vsg geometry
        auto geometry = vsg::Geometry::create();

        geometry->_arrays = attributeArrays;
        geometry->_indices = vsgindices;

        vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(vsgindices->valueCount(), 1, 0, 0, 0);
        geometry->_commands = vsg::Geometry::Commands{ drawIndexed };

        return geometry;
    }

    vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGeometryGraphicsPipeline(const uint32_t& geometryAttributesMask, const uint32_t& stateMask, unsigned int maxNumDescriptors)
    {
        //
        // load shaders
        //
        ShaderCompiler shaderCompiler;

        vsg::GraphicsPipelineGroup::Shaders shaders{
            vsg::Shader::create(VK_SHADER_STAGE_VERTEX_BIT, "main", createVertexSource(stateMask, geometryAttributesMask, false)),
            vsg::Shader::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", createFragmentSource(stateMask, geometryAttributesMask, false)),
        };

        if (!shaderCompiler.compile(shaders)) return vsg::ref_ptr<vsg::GraphicsPipelineGroup>();

        //
        // set up graphics pipeline
        //
        vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = vsg::GraphicsPipelineGroup::create();
        gp->shaders = shaders;
        gp->maxSets = maxNumDescriptors;
        gp->descriptorPoolSizes = vsg::DescriptorPoolSizes
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxNumDescriptors} // type, descriptorCount
        };

        gp->descriptorSetLayoutBindings = vsg::DescriptorSetLayoutBindings
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        };

        gp->pushConstantRanges = vsg::PushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
        };

        uint32_t bindingindex = 0;

        vsg::VertexInputState::Bindings vertexBindingsDescriptions = vsg::VertexInputState::Bindings
        {
            VkVertexInputBindingDescription{bindingindex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        };

        vsg::VertexInputState::Attributes vertexAttributeDescriptions = vsg::VertexInputState::Attributes
        {
            VkVertexInputAttributeDescription{VERTEX_CHANNEL, bindingindex, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        };

        bindingindex++;

        if (geometryAttributesMask & NORMAL)
        {
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ bindingindex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ NORMAL_CHANNEL, bindingindex, VK_FORMAT_R32G32B32_SFLOAT, 0 });
            bindingindex++;
        }
        if (geometryAttributesMask & COLOR)
        {
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ bindingindex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX });
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ COLOR_CHANNEL, bindingindex, VK_FORMAT_R32G32B32_SFLOAT, 0 });
            bindingindex++;
        }
        if (geometryAttributesMask & TEXCOORD0)
        {
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ bindingindex, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX });
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ TEXCOORD0_CHANNEL, bindingindex, VK_FORMAT_R32G32_SFLOAT, 0 });
            bindingindex++;
        }

        gp->vertexBindingsDescriptions = vertexBindingsDescriptions;
        gp->vertexAttributeDescriptions = vertexAttributeDescriptions;

        gp->pipelineStates = vsg::GraphicsPipelineStates
        {
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()
        };

        return gp;
    }
}


