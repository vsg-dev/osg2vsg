#include <osg2vsg/ShaderUtils.h>

#include <osg2vsg/GeometryUtils.h>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "glsllang/ResourceLimits.h"


using namespace osg2vsg;

#if 0
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif

uint32_t osg2vsg::calculateShaderModeMask(osg::StateSet* stateSet)
{
    uint32_t stateMask = 0;
    if (stateSet)
    {
        //if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON)
        //    stateMask |= ShaderGen::BLEND;
        if (stateSet->getMode(GL_LIGHTING) & osg::StateAttribute::ON)  stateMask |= LIGHTING;

        auto hasTextureWithImageInChannel = [](osg::StateSet* stateSet, unsigned int channel)
        {
            auto asTex = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(channel, osg::StateAttribute::TEXTURE));
            if(asTex && asTex->getImage(0)) return true;
            return false;
        };

        if (hasTextureWithImageInChannel(stateSet, 0)) stateMask |= DIFFUSE_MAP;
        if (hasTextureWithImageInChannel(stateSet, OPACITY_TEXTURE_UNIT)) stateMask |= OPACITY_MAP;
        if (hasTextureWithImageInChannel(stateSet, AMBIENT_TEXTURE_UNIT)) stateMask |= AMBIENT_MAP;
        if (hasTextureWithImageInChannel(stateSet, NORMAL_TEXTURE_UNIT)) stateMask |= NORMAL_MAP;
        if (hasTextureWithImageInChannel(stateSet, SPECULAR_TEXTURE_UNIT)) stateMask |= SPECULAR_MAP;
    }
    return stateMask;
}

// create vertex shader source using statemask to determine type of shader to build and geometryattributes to determine attribute binding locations

std::string createPSCDefinesString(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;
    bool hastanget = geometryAttrbutes & TANGENT;

    std::string defines = "";
    if (hasnormal) defines += "#define VSG_NORMAL\n";
    if (hascolor) defines += "#define VSG_COLOR\n";
    if (hastex0) defines += "#define VSG_TEXCOORD0\n";
    if (hastanget) defines += "#define VSG_TANGENT\n";
    if (hasnormal && (shaderModeMask & LIGHTING)) defines += "#define VSG_LIGHTING\n";
    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines += "#define VSG_DIFFUSE_MAP\n";
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines += "#define VSG_OPACITY_MAP\n";
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines += "#define VSG_AMBIENT_MAP\n";
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines += "#define VSG_NORMAL_MAP\n";
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines += "#define VSG_SPECULAR_MAP\n";

   return defines;
}

std::string osg2vsg::createVertexSourcePSC(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string defines = createPSCDefinesString(shaderModeMask, geometryAttrbutes);

    std::string source = 
    "#version 450\n" \
    "#extension GL_ARB_separate_shader_objects : enable\n";
    
    source += defines;
    
    source += 
    "layout(push_constant) uniform PushConstants {\n" \
    "    mat4 projection; \n" \
    "    mat4 view;\n" \
    "    mat4 model; \n" \
    "    //mat3 normal;\n" \
    "} pc; \n" \
    "layout(location = 0) in vec3 osg_Vertex;\n" \
    "#if VSG_NORMAL\n" \
    "layout(location = 1) in vec3 osg_Normal;\n" \
    "layout(location = 1) out vec3 normalDir;\n" \
    "#endif\n" \
    "#if VSG_TANGENT\n" \
    "layout(location = 2) in vec4 osg_Tangent;\n" \
    "#endif\n"
    "#if VSG_COLOR\n" \
    "layout(location = 3) in vec4 osg_Color;\n" \
    "layout(location = 3) out vec4 vertColor;\n" \
    "#endif\n"
    "#if VSG_TEXCOORD0\n" \
    "layout(location = 4) in vec2 osg_MultiTexCoord0;\n" \
    "layout(location = 4) out vec2 texCoord0;\n" \
    "#endif\n"
    "#if VSG_LIGHTING\n" \
    "layout(location = 5) out vec3 viewDir;\n" \
    "layout(location = 6) out vec3 lightDir;\n" \
    "#endif\n" \
    "out gl_PerVertex{ vec4 gl_Position; };\n" \
    "\n" \
    "void main()\n" \
    "{\n" \
    "    gl_Position = (pc.projection * pc.view * pc.model) * vec4(osg_Vertex, 1.0);\n" \
    "#if VSG_TEXCOORD0\n" \
    "    texCoord0 = osg_MultiTexCoord0.st;\n" \
    "#endif\n" \
    "#if VSG_LIGHTING\n" \
    "    vec4 lpos = /*osg_LightSource.position*/ vec4(0.0, 0.25, 1.0, 0.0);\n" \
    "    vec3 n = ((pc.view * pc.model) * vec4(osg_Normal, 0.0)).xyz;\n" \

    "    vec3 t = ((pc.view * pc.model) * vec4(osg_Tangent.xyz, 0.0)).xyz;\n" \
    "    vec3 b = cross(n, t);\n" \
    "    vec3 dir = -vec3((pc.view * pc.model) * vec4(osg_Vertex, 1.0));\n" \
    "    viewDir.x = dot(dir, t);\n" \
    "    viewDir.y = dot(dir, b);\n" \
    "    viewDir.z = dot(dir, n);\n" \
    "    if (lpos.w == 0.0)\n" \
    "        dir = lpos.xyz;\n" \
    "    else\n" \
    "        dir += lpos.xyz;\n" \
    "    lightDir.x = dot(dir, t); \n" \
    "    lightDir.y = dot(dir, b);\n" \
    "    lightDir.z = dot(dir, n); \n" \
    "#endif\n" \
    "#if VSG_COLOR\n" \
    "    vertColor = osg_Color;\n" \
    "#endif\n" \
    "}\n";
    return source;
}

std::string osg2vsg::createFragmentSourePSC(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string defines = createPSCDefinesString(shaderModeMask, geometryAttrbutes);

    std::string source =
    "#version 450\n" \
    "#extension GL_ARB_separate_shader_objects : enable\n";

    source += defines;

    source +=
    "layout(binding = 0) uniform sampler2D diffuseMap; \n" \
    "layout(binding = 1) uniform sampler2D opacityMap;\n" \
    "layout(binding = 4) uniform sampler2D ambientMap; \n" \
    "layout(binding = 5) uniform sampler2D normalMap;\n" \
    "layout(binding = 6) uniform sampler2D specularMap; \n" \
    "layout(location = 0) in vec2 texCoord0;\n" \
    "layout(location = 1) in vec3 normalDir; \n" \
    "layout(location = 2) in vec3 viewDir; \n" \
    "layout(location = 3) in vec3 lightDir;\n" \
    "layout(location = 4) in vec4 vertColor; \n" \
    "layout(location = 0) out vec4 outColor;\n" \
    "\n" \
    "void main()\n" \
    "{\n" \
    "    vec4 base = texture(diffuseMap, texCoord0.st);\n" \
    "    base = base * vertColor;\n" \
    "    vec3 normalDir = texture(normalMap, texCoord0.st).xyz*2.0 - 1.0;\n" \
    "    normalDir.g = -normalDir.g;\n" \
    "    vec3 specularColor = texture(specularMap, texCoord0.st).rgb;\n" \
    "    float ambientOcclusion = texture(ambientMap, texCoord0.st).r;\n" \
    "    vec3 nd = normalize(normalDir);\n" \
    "    vec3 ld = normalize(lightDir);\n" \
    "    vec3 vd = normalize(viewDir);\n" \
    "    vec4 color = vec4(0.01, 0.01, 0.01, 1.0);\n" \
    "    color += /*osg_Material.ambient*/ vec4(0.1, 0.1, 0.1, 0.0);\n" \
    "    float diff = max(dot(ld, nd), 0.0);\n" \
    "    color += /*osg_Material.diffuse*/ vec4(0.8, 0.8, 0.8, 0.0) * diff;\n" \
    "    color *= ambientOcclusion;\n" \
    "    color *= base;\n" \
    "    if (diff > 0.0)\n" \
    "    {\n" \
    "        vec3 halfDir = normalize(ld + vd);\n" \
    "        color.rgb += base.a * specularColor *\n" \
    "            pow(max(dot(halfDir, nd), 0.0), 16.0/*osg_Material.shine*/);\n" \
    "    }\n" \
    "    outColor = color;\n" \
    "    outColor.a *= texture(opacityMap, texCoord0.st).r;\n" \
    "}\n";
    return source;
}


std::string osg2vsg::createVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, bool osgCompatible)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;
    bool hastanget = geometryAttrbutes & TANGENT;

    // regardless of if we use the attribute figure out what it's index should be
    uint32_t inputindex = 0;

    uint32_t vertexindex = osgCompatible ? 0 : VERTEX_CHANNEL;
    uint32_t normalindex = osgCompatible ? 1 : (hasnormal ? NORMAL_CHANNEL : 0);
    uint32_t tangentindex = osgCompatible ? 6 : (hastanget ? TANGENT_CHANNEL : 0);
    uint32_t colorindex = osgCompatible ? 2 : (hascolor ? COLOR_CHANNEL : 0);
    uint32_t tex0index = osgCompatible ? 3 : (hastex0 ? TEXCOORD0_CHANNEL : 0);

    bool uselighting = hasnormal && (shaderModeMask & (LIGHTING));
    bool usediffusemap = hastex0 && (shaderModeMask & (DIFFUSE_MAP));
    bool usenormalmap = hastex0 && uselighting && hastanget && (shaderModeMask & (NORMAL_MAP));


    std::ostringstream uniforms;
    std::ostringstream inputs;
    std::ostringstream outputs;

    // vert uniforms

    std::string mvpmat;
    std::string mvmat;
    std::string nmat;

    if(osgCompatible)
    {
        uniforms << "uniform mat4 osg_ModelViewProjectionMatrix;\n";
        if (uselighting) uniforms << "uniform mat4 osg_ModelViewMatrix;\n";
        if (hasnormal) uniforms << "uniform mat3 osg_NormalMatrix;\n";

        mvpmat = "osg_ModelViewProjectionMatrix";
        mvmat = "osg_ModelViewMatrix";
        nmat = "osg_NormalMatrix";
    }
    else
    {
        uniforms << "layout(push_constant) uniform PushConstants {\n" \
                    "  mat4 projection;\n" \
                    "  mat4 view;\n" \
                    "  mat4 model;\n" \
                    "  //mat3 normal;\n" \
                    "} pc;\n";

        mvpmat = "(pc.projection * pc.view * pc.model)";
        mvmat = "(pc.view * pc.model)";
        nmat = "pc.normal";
    }

    // vert inputs



    inputs << "layout(location = " << vertexindex << ") in vec3 osg_Vertex;\n"; // vertex always at location 0

    if (hasnormal) inputs << "layout(location = " << normalindex << ") in vec3 osg_Normal;\n";

    if (hastanget)
    {
        inputs << "layout(location = " << tangentindex << ") in vec4 osg_Tangent;\n";
    }

    if (hascolor) inputs << "layout(location = " << colorindex << ") in vec4 osg_Color;\n";

    if (hastex0) inputs << "layout(location = " << tex0index << ") in vec2 osg_MultiTexCoord0;\n";


    // vert outputs

    if (hastex0)
    {
        outputs << "layout(location = 0) out vec2 texCoord0;\n";
    }

    // normal dir is only passed for basic lighting, normal mapping we convert our viewdir to tangent space
    if (uselighting && !usenormalmap)
    {
        outputs << "layout(location = 1) out vec3 normalDir;\n";
    }

    if (uselighting || usenormalmap)
    {
        outputs << "layout(location = 2) out vec3 viewDir;\n";
        outputs << "layout(location = 3) out vec3 lightDir;\n";

        /*std::ostringstream lightuniform;
        lightuniform << "struct osg_LightSourceParameters {"
            << "    vec4  position;"
            << "};\n"
            << "uniform osg_LightSourceParameters osg_LightSource;\n";

        uniforms << lightuniform.str();*/
    }

    if (hascolor)
    {
        outputs << "layout(location = 4) out vec4 vertColor;\n";
    }

    if (!osgCompatible)
    {
        outputs << "out gl_PerVertex{ vec4 gl_Position; };\n";
    }

    // Vertex shader code
    std::string header = "#version 450\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n";

    std::ostringstream vert;

    vert << header;

    vert << uniforms.str();
    vert << inputs.str();
    vert << outputs.str();

    vert << "\n"\
        "void main()\n"\
        "{\n"\
        "  gl_Position = " << mvpmat << " * vec4(osg_Vertex, 1.0);\n";

    if (hastex0)
    {
        vert << "  texCoord0 = osg_MultiTexCoord0.st;\n";
    }

    // for now hard code the light position
    if(uselighting || usenormalmap) vert << "  vec4 lpos = /*osg_LightSource.position*/ vec4(0.0,0.25,1.0,0.0);\n";

    if (usenormalmap)
    {
        if(osgCompatible)
        {
            vert <<
                "  vec3 n = " << nmat << " * osg_Normal;\n"\
                "  vec3 t = " << nmat << " * osg_Tangent.xyz;\n"\
                "  vec3 b = cross(n, t);\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir.x = dot(dir, t);\n"\
                "  viewDir.y = dot(dir, b);\n"\
                "  viewDir.z = dot(dir, n);\n";
        }
        else
        {
            vert <<
                "  vec3 n = (" << mvmat << " * vec4(osg_Normal, 0.0)).xyz;\n"\
                "  vec3 t = (" << mvmat << " * vec4(osg_Tangent.xyz, 0.0)).xyz;\n"\
                "  vec3 b = cross(n, t);\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir.x = dot(dir, t);\n"\
                "  viewDir.y = dot(dir, b);\n"\
                "  viewDir.z = dot(dir, n);\n";
        }
        vert <<
            "  if (lpos.w == 0.0)\n"\
            "    dir = lpos.xyz;\n"\
            "  else\n"\
            "    dir += lpos.xyz;\n"\
            "  lightDir.x = dot(dir, t);\n"\
            "  lightDir.y = dot(dir, b);\n"\
            "  lightDir.z = dot(dir, n);\n";
    }
    else if (uselighting)
    {
        if(osgCompatible)
        {
            vert <<
                "  normalDir = " << nmat << " * osg_Normal;\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir = dir;\n";
        }
        else
        {
            vert <<
                "  normalDir = (" << mvmat << " * vec4(osg_Normal, 0.0)).xyz;\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir = dir;\n";
        }
        vert <<
            "  if (lpos.w == 0.0)\n"\
            "    lightDir = lpos.xyz;\n"\
            "  else\n"\
            "    lightDir = lpos.xyz + dir;\n";
    }

    if(hascolor)
    {
        vert << "  vertColor = osg_Color;\n";
    }

    vert << "}\n";

    return vert.str();
}

std::string osg2vsg::createFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, bool osgCompatible, bool setPerTexture)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;

    bool hasdiffusemap = shaderModeMask & DIFFUSE_MAP;
    bool hasopacitymap = shaderModeMask & OPACITY_MAP;
    bool hasambientmap = shaderModeMask & AMBIENT_MAP;
    bool hasnormalmap = shaderModeMask & NORMAL_MAP;
    bool hasspecularmap = shaderModeMask & SPECULAR_MAP;

    bool uselighting = hasnormal && (shaderModeMask & (LIGHTING));
    bool usediffusemap = hastex0 && hasdiffusemap;
    bool useopacitymap = hastex0 && hasopacitymap;
    bool useambientmap = hastex0 && uselighting && hasambientmap;
    bool usenormalmap = hastex0 && uselighting && hasnormalmap;
    bool usespecularmap = hastex0 && hasspecularmap;

    std::ostringstream uniforms;
    std::ostringstream inputs;
    std::ostringstream outputs;

    // inputs

    if (hastex0)
    {
        inputs << "layout(location = 0) in vec2 texCoord0;\n";
    }

    // normal dir is only passed for basic lighting, normal mapping we convert our viewdir to tangent space
    if (uselighting && !usenormalmap)
    {
        inputs << "layout(location = 1) in vec3 normalDir;\n";
    }

    if (uselighting || usenormalmap)
    {
        inputs << "layout(location = 2) in vec3 viewDir;\n";
        inputs << "layout(location = 3) in vec3 lightDir;\n";

        /*std::ostringstream lightuniform;
        lightuniform << "struct osg_LightSourceParameters {"
            << "    vec4  position;"
            << "};\n"
            << "uniform osg_LightSourceParameters osg_LightSource;\n";

        uniforms << lightuniform.str();*/
    }

    if (hascolor)
    {
        inputs << "layout(location = 4) in vec4 vertColor;\n";
    }

    // uniforms

    if (uselighting || usenormalmap)
    {
        /*uniforms << "struct osgMaterial{\n"\
            "  vec4 ambient;\n"\
            "  vec4 diffuse;\n"\
            "  vec4 specular;\n"\
            "  float shine;\n"\
            "};\n"\
            "uniform osgMaterial osg_Material;\n";*/
    }

    if(setPerTexture)
    {
        int setIndex = 0;
        if (hasdiffusemap)
        {
            uniforms << "layout(set = " << setIndex++ << ", binding = 0) uniform sampler2D diffuseMap;\n";
        }

        if (hasopacitymap)
        {
            uniforms << "layout(set = " << setIndex++ << ", binding = 0) uniform sampler2D opacityMap;\n";
        }

        if (hasambientmap)
        {
            uniforms << "layout(set = " << setIndex++ << ", binding = 0) uniform sampler2D ambientMap;\n";
        }

        if (hasnormalmap)
        {
            uniforms << "layout(set = " << setIndex++ << ", binding = 0) uniform sampler2D normalMap;\n";
        }

        if (hasspecularmap)
        {
            uniforms << "layout(set = " << setIndex++ << ", binding = 0) uniform sampler2D specularMap;\n";
        }
    }
    else
    {
        if (hasdiffusemap)
        {
            uniforms << "layout(binding = " << DIFFUSE_TEXTURE_UNIT << ") uniform sampler2D diffuseMap;\n";
        }

        if (hasopacitymap)
        {
            uniforms << "layout(binding = " << OPACITY_TEXTURE_UNIT << ") uniform sampler2D opacityMap;\n";
        }

        if (hasambientmap)
        {
            uniforms << "layout(binding = " << AMBIENT_TEXTURE_UNIT << ") uniform sampler2D ambientMap;\n";
        }

        if (hasnormalmap)
        {
            uniforms << "layout(binding = " << NORMAL_TEXTURE_UNIT << ") uniform sampler2D normalMap;\n";
        }

        if (hasspecularmap)
        {
            uniforms << "layout(binding = " << SPECULAR_TEXTURE_UNIT << ") uniform sampler2D specularMap;\n";
        }
    }

    // outputs

    outputs << "layout(location = 0) out vec4 outColor;\n";


    std::ostringstream frag;

    std::string header = "#version 450\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n";

    frag << header;

    frag << uniforms.str();
    frag << inputs.str();
    frag << outputs.str();

    frag << "\n"\
        "void main()\n"\
        "{\n";

    if (usediffusemap)
    {
        frag << "  vec4 base = texture(diffuseMap, texCoord0.st);\n";
    }
    else
    {
        frag << "  vec4 base = vec4(1.0);\n";
    }

    if(hascolor)
    {
        frag << "  base = base * vertColor;\n";
    }

    if (usenormalmap)
    {
        frag << "  vec3 normalDir = texture(normalMap, texCoord0.st).xyz*2.0-1.0;\n";
        frag << " normalDir.g = -normalDir.g;\n";
    }

    if (uselighting || usenormalmap)
    {
        frag << "vec3 specularColor = " << (usespecularmap ? "texture(specularMap, texCoord0.st).rgb" : "vec3(0.2,0.2,0.2)") << ";\n";

        frag << "float ambientOcclusion = " << (useambientmap ? "texture(ambientMap, texCoord0.st).r" : "1.0") << ";\n";

        frag <<
            "  vec3 nd = normalize(normalDir);\n"\
            "  vec3 ld = normalize(lightDir);\n"\
            "  vec3 vd = normalize(viewDir);\n"\
            "  vec4 color = vec4(0.01,0.01,0.01,1.0);\n"\
            "  color += /*osg_Material.ambient*/ vec4(0.1,0.1,0.1,0.0);\n"\
            "  float diff = max(dot(ld, nd), 0.0);\n"\
            "  color += /*osg_Material.diffuse*/ vec4(0.8,0.8,0.8,0.0) * diff;\n"\
            "  color *= ambientOcclusion;\n"\
            "  color *= base;\n"\
            "  if (diff > 0.0)\n"\
            "  {\n"\
            "    vec3 halfDir = normalize(ld+vd);\n"\
            "    color.rgb += base.a * specularColor * \n"\
            "      pow(max(dot(halfDir, nd), 0.0), 16.0/*osg_Material.shine*/);\n"\
            "  }\n";
    }
    else
    {
        frag << "  vec4 color = base;\n";
    }

    frag << "  outColor = color;\n";

    if (useopacitymap)
    {
        frag << "  outColor.a *= texture(opacityMap, texCoord0.st).r;\n";
    }

    frag << "}\n";

    return frag.str();
}

ShaderCompiler::ShaderCompiler(vsg::Allocator* allocator):
    vsg::Object(allocator)
{
    glslang::InitializeProcess();
}

ShaderCompiler::~ShaderCompiler()
{
    glslang::FinalizeProcess();
}

bool ShaderCompiler::compile(Shaders& shaders)
{
    using StageShaderMap = std::map<EShLanguage, vsg::ref_ptr<vsg::Shader>>;
    using TShaders = std::list<std::unique_ptr<glslang::TShader>>;
    TShaders tshaders;

    TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;

    StageShaderMap stageShaderMap;
    std::unique_ptr<glslang::TProgram> program(new glslang::TProgram);

    for(auto& vsg_shader : shaders)
    {
        EShLanguage envStage = EShLangCount;
        switch(vsg_shader->stage())
        {
            case(VK_SHADER_STAGE_VERTEX_BIT): envStage = EShLangVertex; break;
            case(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): envStage = EShLangTessControl; break;
            case(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): envStage = EShLangTessEvaluation; break;
            case(VK_SHADER_STAGE_GEOMETRY_BIT): envStage = EShLangGeometry; break;
            case(VK_SHADER_STAGE_FRAGMENT_BIT) : envStage = EShLangFragment; break;
            case(VK_SHADER_STAGE_COMPUTE_BIT): envStage = EShLangCompute; break;
    #ifdef VK_SHADER_STAGE_RAYGEN_BIT_NV
            case(VK_SHADER_STAGE_RAYGEN_BIT_NV): envStage = EShLangRayGenNV; break;
            case(VK_SHADER_STAGE_ANY_HIT_BIT_NV): envStage = EShLangAnyHitNV; break;
            case(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV): envStage = EShLangClosestHitNV; break;
            case(VK_SHADER_STAGE_MISS_BIT_NV): envStage = EShLangMissNV; break;
            case(VK_SHADER_STAGE_INTERSECTION_BIT_NV): envStage = EShLangIntersectNV; break;
            case(VK_SHADER_STAGE_CALLABLE_BIT_NV): envStage = EShLangCallableNV; break;
            case(VK_SHADER_STAGE_TASK_BIT_NV): envStage = EShLangTaskNV; ;
            case(VK_SHADER_STAGE_MESH_BIT_NV): envStage = EShLangMeshNV; break;
    #endif
            default: break;
        }

        if (envStage==EShLangCount) return false;

        glslang::TShader* shader(new glslang::TShader(envStage));
        tshaders.emplace_back(shader);

        shader->setEnvInput(glslang::EShSourceGlsl, envStage, glslang::EShClientVulkan, 150);
        shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

        const char* str = vsg_shader->source().c_str();
        shader->setStrings(&str, 1);

        int defaultVersion = 110; // 110 desktop, 100 non desktop
        bool forwardCompatible = false;
        EShMessages messages = EShMsgDefault;
        bool parseResult = shader->parse(&builtInResources, defaultVersion, forwardCompatible,  messages);

        if (parseResult)
        {
            program->addShader(shader);

            stageShaderMap[envStage] = vsg_shader;
        }
        else
        {
            DEBUG_OUTPUT << "glslLang: Error parsing shader: " << shader->getInfoLog() << std::endl;
        }
    }

    if (stageShaderMap.empty())
    {
        DEBUG_OUTPUT<<"ShaderCompiler::compile(Shaders& shaders) stageShaderMap.empty()"<<std::endl;
        return false;
    }

    EShMessages messages = EShMsgDefault;
    if (!program->link(messages))
    {
        return false;
    }

    for (int eshl_stage = 0; eshl_stage < EShLangCount; ++eshl_stage)
    {
        auto vsg_shader = stageShaderMap[(EShLanguage)eshl_stage];
        if (vsg_shader && program->getIntermediate((EShLanguage)eshl_stage))
        {
            vsg::Shader::SPIRV spirv;
            std::string warningsErrors;
            spv::SpvBuildLogger logger;
            glslang::SpvOptions spvOptions;
            glslang::GlslangToSpv(*(program->getIntermediate((EShLanguage)eshl_stage)), vsg_shader->spirv(), &logger, &spvOptions);
        }
    }

    return true;
}
