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


    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec2Array> convertToVsg(osg::Vec2Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec3Array> convertToVsg(osg::Vec3Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec4Array> convertToVsg(osg::Vec4Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Data> convertToVsg(osg::Array* inarray);

    extern OSG2VSG_DECLSPEC uint32_t calculateAttributesMask(osg::Geometry* geometry);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* geometry, uint32_t requiredAttributesMask = 0);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGeometryGraphicsPipeline(const uint32_t& geometryAttributesMask, vsg::Paths& searchPaths);
}