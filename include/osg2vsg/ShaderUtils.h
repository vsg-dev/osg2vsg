#pragma once

#include <osg2vsg/Export.h>

#include <vsg/all.h>

#include <osg/Array>
#include <osg/StateSet>

namespace osg2vsg
{
    enum StateMask
    {
        NONE = 0,
        LIGHTING = 1,
        DIFFUSE_MAP = 2, //< Texture in unit 0
        NORMAL_MAP = 4  //< Texture in unit 1 and tangent vector array in index 6
    };

    // taken from osg fbx plugin
    enum TextureUnit
    {
        DIFFUSE_TEXTURE_UNIT = 0,
        OPACITY_TEXTURE_UNIT,
        REFLECTION_TEXTURE_UNIT,
        EMISSIVE_TEXTURE_UNIT,
        AMBIENT_TEXTURE_UNIT,
        NORMAL_TEXTURE_UNIT,
        SPECULAR_TEXTURE_UNIT,
        SHININESS_TEXTURE_UNIT
    };

    // create vertex shader source using statemask to determine type of shader to build and geometryattributes to determine attribute binding locations
    extern OSG2VSG_DECLSPEC std::string createVertexSource(const uint32_t& stateMask, const uint32_t& geometryAttrbutes, bool osgCompatible);

    extern OSG2VSG_DECLSPEC std::string createFragmentSource(const uint32_t& stateMask, const uint32_t& geometryAttrbutes, bool osgCompatible);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Shader> compileSourceToSPV(const std::string& source, bool isvert);
}