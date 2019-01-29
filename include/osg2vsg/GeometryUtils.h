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
        VERTEX,
        NORMAL,
        COLOR,
        TEXCOORD0,
        TEXCOORD1,
        TEXCOORD2
    };


    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec2Array> convertToVsg(osg::Vec2Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec3Array> convertToVsg(osg::Vec3Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::vec4Array> convertToVsg(osg::Vec4Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Data> convertToVsg(osg::Array* inarray);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* geometry);
}