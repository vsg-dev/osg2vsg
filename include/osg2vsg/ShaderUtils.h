#pragma once

#include <osg2vsg/Export.h>
#include <osg2vsg/GraphicsNodes.h>

#include <vsg/all.h>

#include <osg/Array>
#include <osg/StateSet>

namespace osg2vsg
{
    enum ShaderModeMask : uint32_t
    {
        NONE = 0,
        LIGHTING = 1,
        DIFFUSE_MAP = 2,
        OPACITY_MAP = 4,
        AMBIENT_MAP = 8,
        NORMAL_MAP = 16,
        SPECULAR_MAP = 32,
        ALL_SHADER_MODE_MASK = LIGHTING | DIFFUSE_MAP | OPACITY_MAP | AMBIENT_MAP | SPECULAR_MAP
    };

    // taken from osg fbx plugin
    enum TextureUnit : uint32_t
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

    extern OSG2VSG_DECLSPEC uint32_t calculateShaderModeMask(osg::StateSet* stateSet);

    // create vertex shader source using statemask to determine type of shader to build and geometryattributes to determine attribute binding locations
    extern OSG2VSG_DECLSPEC std::string createVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, bool osgCompatible);

    extern OSG2VSG_DECLSPEC std::string createFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, bool osgCompatible);

    extern OSG2VSG_DECLSPEC vsg::ref_ptr<vsg::Shader> compileSourceToSPV(const std::string& source, bool isvert);

    class OSG2VSG_DECLSPEC ShaderCompiler : public vsg::Object
    {
    public:
        ShaderCompiler(vsg::Allocator* allocator=nullptr);
        virtual ~ShaderCompiler();

        using Shaders = vsg::GraphicsPipelineGroup::Shaders;

        bool compile(vsg::Shader* shader);
        bool compile(Shaders& shaders);
    };
}
