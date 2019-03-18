#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/ShaderUtils.h>

#include <vsg/nodes/StateGroup.h>

#include <osgUtil/MeshOptimizers>
#include <osgUtil/TangentSpaceGenerator>

namespace osg2vsg
{

    vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec2Array>();

        uint32_t count = inarray->size();
        vsg::ref_ptr<vsg::vec2Array> outarray(new vsg::vec2Array(count));
        std::memcpy(outarray->dataPointer(), inarray->getDataPointer(), outarray->dataSize());
        return outarray;
    }

    vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec3Array>();

        uint32_t count = inarray->size();
        vsg::ref_ptr<vsg::vec3Array> outarray(new vsg::vec3Array(count));
        std::memcpy(outarray->dataPointer(), inarray->getDataPointer(), outarray->dataSize());
        return outarray;
    }

    vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray)
    {
        if (!inarray) return vsg::ref_ptr<vsg::vec4Array>();

        uint32_t count = inarray->size();
        vsg::ref_ptr<vsg::vec4Array> outarray(new vsg::vec4Array(count));
        std::memcpy(outarray->dataPointer(), inarray->getDataPointer(), outarray->dataSize());
        return outarray;
    }

    vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray)
    {
        if (!inarray) return vsg::ref_ptr<vsg::Data>();

        switch (inarray->getType())
        {
            case osg::Array::Type::Vec2ArrayType: return convertToVsg(dynamic_cast<const osg::Vec2Array*>(inarray));
            case osg::Array::Type::Vec3ArrayType: return convertToVsg(dynamic_cast<const osg::Vec3Array*>(inarray));
            case osg::Array::Type::Vec4ArrayType: return convertToVsg(dynamic_cast<const osg::Vec4Array*>(inarray));
            default: return vsg::ref_ptr<vsg::Data>();
        }
    }

    uint32_t calculateAttributesMask(const osg::Geometry* geometry)
    {
        uint32_t mask = 0;
        if(!geometry) return mask;

        if (geometry->getVertexArray() != nullptr) mask |= VERTEX;

        if (geometry->getNormalArray() != nullptr)
        {
            mask |= NORMAL;
            if(geometry->getNormalBinding() == osg::Geometry::AttributeBinding::BIND_OVERALL) mask |= NORMAL_OVERALL;
        }

        if (geometry->getColorArray() != nullptr)
        {
            mask |= COLOR;
            if (geometry->getColorBinding() == osg::Geometry::AttributeBinding::BIND_OVERALL) mask |= COLOR_OVERALL;
        }

        if (geometry->getVertexAttribArray(6) != nullptr)
        {
            mask |= TANGENT;
            if ( geometry->getVertexAttribBinding(6) == osg::Geometry::AttributeBinding::BIND_OVERALL) mask |= TANGENT_OVERALL;
        }

        if (geometry->getTexCoordArray(0) != nullptr) mask |= TEXCOORD0;
        if (geometry->getTexCoordArray(1) != nullptr) mask |= TEXCOORD1;
        if (geometry->getTexCoordArray(2) != nullptr) mask |= TEXCOORD2;
        return mask;
    }

    VkPrimitiveTopology convertToTopology(osg::PrimitiveSet::Mode primitiveMode)
    {
        switch (primitiveMode)
        {
            case osg::PrimitiveSet::Mode::POINTS: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case osg::PrimitiveSet::Mode::LINES: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case osg::PrimitiveSet::Mode::LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case osg::PrimitiveSet::Mode::TRIANGLES: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

    VkSamplerAddressMode covertToSamplerAddressMode(osg::Texture::WrapMode wrapmode)
    {
        switch (wrapmode)
        {
            case osg::Texture::WrapMode::CLAMP: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case osg::Texture::WrapMode::CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case osg::Texture::WrapMode::CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case osg::Texture::WrapMode::REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case osg::Texture::WrapMode::MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                //VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE no osg equivelent for this
            default: return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM; // unknown
        }
    }

    std::pair<VkFilter, VkSamplerMipmapMode> convertToFilterAndMipmapMode(osg::Texture::FilterMode filtermode)
    {
        switch (filtermode)
        {
            case osg::Texture::FilterMode::LINEAR: return { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST };
            case osg::Texture::FilterMode::LINEAR_MIPMAP_LINEAR: return { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR };
            case osg::Texture::FilterMode::LINEAR_MIPMAP_NEAREST: return { VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST };
            case osg::Texture::FilterMode::NEAREST: return { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST };
            case osg::Texture::FilterMode::NEAREST_MIPMAP_LINEAR: return { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR };
            case osg::Texture::FilterMode::NEAREST_MIPMAP_NEAREST: return { VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST };
            default: return { VK_FILTER_MAX_ENUM, VK_SAMPLER_MIPMAP_MODE_MAX_ENUM }; // unknown
        }
    }

    VkSamplerCreateInfo convertToSamplerCreateInfo(const osg::Texture* texture)
    {
        auto minFilterMipmapMode = convertToFilterAndMipmapMode(texture->getFilter(osg::Texture::FilterParameter::MIN_FILTER));
        auto magFilterMipmapMode = convertToFilterAndMipmapMode(texture->getFilter(osg::Texture::FilterParameter::MAG_FILTER));

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = minFilterMipmapMode.first;
        samplerInfo.magFilter = magFilterMipmapMode.first;
        samplerInfo.addressModeU = covertToSamplerAddressMode(texture->getWrap(osg::Texture::WrapParameter::WRAP_S));
        samplerInfo.addressModeV = covertToSamplerAddressMode(texture->getWrap(osg::Texture::WrapParameter::WRAP_T));
        samplerInfo.addressModeW = covertToSamplerAddressMode(texture->getWrap(osg::Texture::WrapParameter::WRAP_R));
#if 1
        // requres Logical device to have deviceFeatures.samplerAnisotropy = VK_TRUE; set when creating the vsg::Deivce
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
#else
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1;
#endif
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = minFilterMipmapMode.second; // should we use min or mag?

        return samplerInfo;
    }

    vsg::ref_ptr<vsg::MaterialValue> convertToMaterialValue(const osg::Material* material)
    {
        vsg::ref_ptr<vsg::MaterialValue> matvalue(new vsg::MaterialValue);

        osg::Vec4 ambient = material->getAmbient(osg::Material::Face::FRONT);
        if (ambient.x() == 1.0f && ambient.y() == 1.0f && ambient.z() == 1.0f) ambient.set(0.1f, 0.1f, 0.1f, 1.0f); // ambient sanity check (shouldn't be needed but models seem to have bad material values)
        matvalue->value().ambientColor = vsg::vec4(ambient.x(), ambient.y(), ambient.z(), ambient.w());

        osg::Vec4 diffuse = material->getDiffuse(osg::Material::Face::FRONT);
        if(diffuse.x() == 0.0f && diffuse.y() == 0.0f && diffuse.z() == 0.0f) diffuse.set(1.0f,1.0f,1.0f,1.0f); // diffuse sanity check (shouldn't be needed but models seem to have bad material values)
        matvalue->value().diffuseColor = vsg::vec4(diffuse.x(), diffuse.y(), diffuse.z(), diffuse.w());

        osg::Vec4 specular = material->getSpecular(osg::Material::Face::FRONT);
        matvalue->value().specularColor = vsg::vec4(specular.x(), specular.y(), specular.z(), specular.w());

        float shine = material->getShininess(osg::Material::Face::FRONT);
        matvalue->value().shine = shine;

        return matvalue;
    }

    vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* ingeometry, uint32_t requiredAttributesMask)
    {
        // convert attribute arrays, create defaults for any requested that don't exist for now to ensure pipline gets required data
        vsg::ref_ptr<vsg::Data> vertices(osg2vsg::convertToVsg(ingeometry->getVertexArray()));
        if (!vertices.valid() || vertices->valueCount() == 0) return vsg::ref_ptr<vsg::Geometry>();

        // normals
        vsg::ref_ptr<vsg::Data> normals(osg2vsg::convertToVsg(ingeometry->getNormalArray()));

        // tangents
        vsg::ref_ptr<vsg::Data> tangents(osg2vsg::convertToVsg(ingeometry->getVertexAttribArray(6)));
        if ((!tangents.valid() || tangents->valueCount() == 0) && (requiredAttributesMask & TANGENT))
        {
            osg::ref_ptr<osgUtil::TangentSpaceGenerator> tangentSpaceGenerator = new osgUtil::TangentSpaceGenerator();
            tangentSpaceGenerator->generate(ingeometry, 0);

            osg::Vec4Array* tangentArray = tangentSpaceGenerator->getTangentArray();
            //osg::Vec4Array* biNormalArray = tangGen->getBinormalArray();

            if (tangentArray && tangentArray->size() > 0)
            {
                tangents = osg2vsg::convertToVsg(tangentArray);
                // bind them to the osg geometry too??
                ingeometry->setVertexAttribArray(6, tangentArray);
                ingeometry->setVertexAttribBinding(6, osg::Geometry::BIND_PER_VERTEX);
            }
        }

        // colors
        vsg::ref_ptr<vsg::Data> colors(osg2vsg::convertToVsg(ingeometry->getColorArray()));

        // tex0
        vsg::ref_ptr<vsg::Data> texcoord0(osg2vsg::convertToVsg(ingeometry->getTexCoordArray(0)));

        // fill arrays data list THE ORDER HERE IS IMPORTANT
        auto attributeArrays = vsg::DataList{ vertices }; // always have verticies
        if (normals.valid() && normals->valueCount() > 0) attributeArrays.push_back(normals);
        if (tangents.valid() && tangents->valueCount() > 0) attributeArrays.push_back(tangents);
        if (colors.valid() && colors->valueCount() > 0) attributeArrays.push_back(colors);
        if (texcoord0.valid() && texcoord0->valueCount() > 0) attributeArrays.push_back(texcoord0);

        // convert indicies

        // asume all the draw elements use the same primitive mode, copy all drawelements indicies into one indicie array and use in single drawindexed command
        // create a draw command per drawarrays primitive set

        vsg::Geometry::Commands drawCommands;

        std::vector<uint16_t> indcies; // use to combine indicies from all drawelements
        osg::Geometry::PrimitiveSetList& primitiveSets = ingeometry->getPrimitiveSetList();
        for (osg::Geometry::PrimitiveSetList::const_iterator itr = primitiveSets.begin();
            itr != primitiveSets.end();
            ++itr)
        {
            osg::DrawElements* de = (*itr)->getDrawElements();
            if (de)
            {
                // merge indicies
                auto numindcies = de->getNumIndices();
                for (unsigned int i = 0; i < numindcies; i++)
                {
                    indcies.push_back(de->index(i));
                }
            }
            else
            {
                // see if we have a drawarrays and create a draw command
                if ((*itr)->getType() == osg::PrimitiveSet::Type::DrawArraysPrimitiveType) //  asDrawArrays != nullptr)
                {
                    osg::DrawArrays* da = dynamic_cast<osg::DrawArrays*>((*itr).get());

                    drawCommands.push_back(vsg::Draw::create(da->getCount(), 1, da->getFirst(), 0));
                }
            }
        }

        // create the vsg geometry
        auto geometry = vsg::Geometry::create();

        geometry->_arrays = attributeArrays;

        // copy into ushortArray
        if(indcies.size() > 0)
        {
            vsg::ref_ptr<vsg::ushortArray> vsgindices(new vsg::ushortArray(indcies.size()));
            std::copy(indcies.begin(), indcies.end(), reinterpret_cast<uint16_t*>(vsgindices->dataPointer()));

            geometry->_indices = vsgindices;

            drawCommands.push_back(vsg::DrawIndexed::create(vsgindices->valueCount(), 1, 0, 0, 0));
        }

        geometry->_commands = drawCommands;

        return geometry;
    }

    vsg::ref_ptr<vsg::Texture> convertToVsgTexture(const osg::Texture* osgtexture)
    {
        const osg::Image* image = osgtexture ? osgtexture->getImage(0) : nullptr;
        auto textureData = convertToVsg(image);
        if (!textureData)
        {
            // DEBUG_OUTPUT << "Could not convert osg image data" << std::endl;
            return vsg::ref_ptr<vsg::Texture>();
        }
        vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
        texture->_textureData = textureData;
        texture->_samplerInfo = convertToSamplerCreateInfo(osgtexture);

        return texture;
    }

    vsg::ref_ptr<vsg::DescriptorSet> createVsgStateSet(const vsg::DescriptorSetLayouts& descriptorSetLayouts, const osg::StateSet* stateset, uint32_t shaderModeMask)
    {
        if (!stateset) return vsg::ref_ptr<vsg::DescriptorSet>();

        uint32_t texcount = 0;

        vsg::Descriptors descriptors;

        auto addTexture = [&] (unsigned int i)
        {
            const osg::StateAttribute* texatt = stateset->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
            const osg::Texture* osgtex = dynamic_cast<const osg::Texture*>(texatt);
            if (osgtex)
            {
                vsg::ref_ptr<vsg::Texture> vsgtex = convertToVsgTexture(osgtex);
                if (vsgtex)
                {
                    // shaders are looking for textures in original units
                    vsgtex->_dstBinding = i;
                    texcount++; //
                    descriptors.push_back(vsgtex);
                }
                else
                {
                    std::cout<<"createVsgStateSet(..) osg::Texture, with i="<<i<<" found but cannot be mapped to vsg::Texture."<<std::endl;
                }
            }
        };

        // add material first
        const osg::Material* osg_material = dynamic_cast<const osg::Material*>(stateset->getAttribute(osg::StateAttribute::Type::MATERIAL));
        if (osg_material != nullptr && stateset->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON)
        {
            vsg::ref_ptr<vsg::MaterialValue> matdata = convertToMaterialValue(osg_material);
            vsg::ref_ptr<vsg::Uniform> vsg_materialUniform = vsg::Uniform::create();
            vsg_materialUniform->_dataList.push_back(matdata);
            vsg_materialUniform->_dstBinding = 10; // just use high value for now, should maybe put uniforms into a different descriptor set to simplify binding indexes
            descriptors.push_back(vsg_materialUniform);
        }

        // add textures
        if (shaderModeMask & ShaderModeMask::DIFFUSE_MAP) addTexture(DIFFUSE_TEXTURE_UNIT);
        if (shaderModeMask & ShaderModeMask::OPACITY_MAP) addTexture(OPACITY_TEXTURE_UNIT);
        if (shaderModeMask & ShaderModeMask::AMBIENT_MAP) addTexture(AMBIENT_TEXTURE_UNIT);
        if (shaderModeMask & ShaderModeMask::NORMAL_MAP) addTexture(NORMAL_TEXTURE_UNIT);
        if (shaderModeMask & ShaderModeMask::SPECULAR_MAP) addTexture(SPECULAR_TEXTURE_UNIT);

        if (descriptors.size() == 0) return vsg::ref_ptr<vsg::DescriptorSet>();

        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayouts, descriptors);

        return descriptorSet;
    }


    vsg::ref_ptr<vsg::BindGraphicsPipeline> createBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryAttributesMask, const std::string& vertShaderPath, const std::string& fragShaderPath)
    {
        //
        // load shaders
        //
        ShaderCompiler shaderCompiler;

        vsg::ShaderModules shaders{
            vsg::ShaderModule::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertShaderPath.empty() ? createFbxVertexSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(vertShaderPath, shaderModeMask, geometryAttributesMask)),
            vsg::ShaderModule::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderPath.empty() ? createFbxFragmentSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(fragShaderPath, shaderModeMask, geometryAttributesMask))
        };

        if (!shaderCompiler.compile(shaders)) return vsg::ref_ptr<vsg::BindGraphicsPipeline>();

        // std::cout<<"createBindGraphicsPipeline("<<shaderModeMask<<", "<<geometryAttributesMask<<")"<<std::endl;

        vsg::DescriptorSetLayoutBindings descriptorBindings;

        // add material first if any (for now material is hardcoded to binding 10)
        if (shaderModeMask & MATERIAL) descriptorBindings.push_back({ 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}

        // these need to go in incremental order by texture unit value as that how they will have been added to the desctiptor set
        // VkDescriptorSetLayoutBinding { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        if (shaderModeMask & DIFFUSE_MAP) descriptorBindings.push_back({ DIFFUSE_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr }); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        if (shaderModeMask & OPACITY_MAP) descriptorBindings.push_back({ OPACITY_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
        if (shaderModeMask & AMBIENT_MAP) descriptorBindings.push_back({ AMBIENT_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
        if (shaderModeMask & NORMAL_MAP) descriptorBindings.push_back({ NORMAL_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });
        if (shaderModeMask & SPECULAR_MAP) descriptorBindings.push_back({ SPECULAR_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

        vsg::PushConstantRanges pushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
        };

        uint32_t vertexBindingIndex = 0;

        vsg::VertexInputState::Bindings vertexBindingsDescriptions = vsg::VertexInputState::Bindings
        {
            VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        };

        vsg::VertexInputState::Attributes vertexAttributeDescriptions = vsg::VertexInputState::Attributes
        {
            VkVertexInputAttributeDescription{VERTEX_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        };

        vertexBindingIndex++;

        if (geometryAttributesMask & NORMAL)
        {
            VkVertexInputRate nrate = geometryAttributesMask & NORMAL_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec3), nrate});
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ NORMAL_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0 }); // normal as vec3
            vertexBindingIndex++;
        }
        if (geometryAttributesMask & TANGENT)
        {
            VkVertexInputRate trate = geometryAttributesMask & TANGENT_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec4), trate });
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ TANGENT_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }); // tanget as vec4
            vertexBindingIndex++;
        }
        if (geometryAttributesMask & COLOR)
        {
            VkVertexInputRate crate = geometryAttributesMask & COLOR_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec4), crate });
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ COLOR_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }); // color as vec4
            vertexBindingIndex++;
        }
        if (geometryAttributesMask & TEXCOORD0)
        {
            vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{ vertexBindingIndex, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX });
            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{ TEXCOORD0_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32_SFLOAT, 0 }); // texcoord as vec2
            vertexBindingIndex++;
        }

        auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

        vsg::GraphicsPipelineStates pipelineStates
        {
            vsg::ShaderStages::create(shaders),
            vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()
        };

        //
        // set up graphics pipeline
        //
        vsg::ref_ptr<vsg::GraphicsPipeline> graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, pipelineStates);
        auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

        return bindGraphicsPipeline;
    }
}

