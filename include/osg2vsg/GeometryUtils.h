#pragma once

#include <osg2vsg/Export.h>
#include <osg2vsg/GraphicsNodes.h>
#include <osg2vsg/StateAttributes.h>

#include <vsg/all.h>

#include <osg/Array>
#include <osg/Geometry>

namespace osg2vsg
{
    enum GeometryAttributes : uint32_t
    {
        VERTEX = 1,
        NORMAL = 2,
        TANGENT = 4,
        COLOR = 8,
        TEXCOORD0 = 16,
        TEXCOORD1 = 32,
        TEXCOORD2 = 64,
        STANDARD_ATTS = VERTEX | NORMAL | COLOR | TEXCOORD0,
        ALL_ATTS = VERTEX | NORMAL | COLOR | TEXCOORD0 | TEXCOORD1 | TEXCOORD2 | TANGENT
    };

    enum AttributeChannels : uint32_t
    {
        VERTEX_CHANNEL = 0,  // osg 0
        NORMAL_CHANNEL = 1, // osg 1
        TANGENT_CHANNEL = 2, //osg 6
        COLOR_CHANNEL = 3, // osg 2
        TEXCOORD0_CHANNEL = 4, //osg 3
        TEXCOORD1_CHANNEL = TEXCOORD0_CHANNEL + 1,
        TEXCOORD2_CHANNEL = TEXCOORD0_CHANNEL + 2,
    };

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC uint32_t calculateAttributesMask(const osg::Geometry* geometry);

    extern OSG2VSG_DECLSPEC VkPrimitiveTopology convertToTopology(osg::PrimitiveSet::Mode primitiveMode);

    extern OSG2VSG_DECLSPEC VkSamplerAddressMode covertToSamplerAddressMode(osg::Texture::WrapMode wrapmode);

    extern OSG2VSG_DECLSPEC std::pair<VkFilter, VkSamplerMipmapMode> convertToFilterAndMipmapMode(osg::Texture::FilterMode filtermode);

    extern OSG2VSG_DECLSPEC VkSamplerCreateInfo convertToSamplerCreateInfo(const osg::Texture* texture);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::AttributesNode> createTextureAttributesNode(const osg::StateSet* stateset);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* geometry, uint32_t requiredAttributesMask = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGeometryGraphicsPipeline(const uint32_t& shaderModeMask, const uint32_t& geometryAttributesMask, unsigned int maxNumDescriptors, const std::string& vertShaderPath = "", const std::string& fragShaderPath = "");


    // core VSG style usage
    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Texture> convertToVsgTexture(const osg::Texture* osgtexture);
    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::StateSet> createVsgStateSet(const osg::StateSet* stateset, uint32_t shaderModeMask);
    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::StateSet> createStateSetWithGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryAttributesMask, unsigned int maxNumDescriptors, const std::string& vertShaderPath = "", const std::string& fragShaderPath = "");

}
