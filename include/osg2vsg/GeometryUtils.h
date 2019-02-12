#pragma once

#include <osg2vsg/Export.h>
#include <osg2vsg/GraphicsNodes.h>

#include <vsg/all.h>

#include <osg/Array>
#include <osg/Geometry>

namespace osg2vsg
{
    enum GeometryAttributes
    {
        VERTEX = 1,
        NORMAL = 2,
        COLOR = 4,
        TEXCOORD0 = 8,
        TEXCOORD1 = 16,
        TEXCOORD2 = 32,
        STANDARD_ATTS = VERTEX | NORMAL | COLOR | TEXCOORD0,
        ALL_ATTS = VERTEX | NORMAL | COLOR | TEXCOORD0 | TEXCOORD1 | TEXCOORD2
    };

    enum AttributeChannels
    {
        VERTEX_CHANNEL = 0,
        NORMAL_CHANNEL = 1,
        COLOR_CHANNEL = 2,
        TEXCOORD0_CHANNEL = 3,
        TEXCOORD1_CHANNEL = TEXCOORD0_CHANNEL + 1,
        TEXCOORD2_CHANNEL = TEXCOORD0_CHANNEL + 2,
    };

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray, bool duplicate = false, uint32_t dupcount = 0);

    extern OSG2VSG_DECLSPEC uint32_t calculateAttributesMask(const osg::Geometry* geometry);

    extern OSG2VSG_DECLSPEC VkPrimitiveTopology convertToTopology(osg::PrimitiveSet::Mode primitiveMode);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* geometry, uint32_t requiredAttributesMask = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGeometryGraphicsPipeline(const uint32_t& geometryAttributesMask, const uint32_t& stateMask, unsigned int maxNumDescriptors);

}
