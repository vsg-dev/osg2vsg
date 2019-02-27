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
            if (asTex && asTex->getImage(0)) return true;
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

// create defines string based of shader mask

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

// insert defines string after the version in source

std::string insertDefinesInShaderSource(const std::string& source, const std::string& defines)
{
    // trim leading spaces
    auto trimLeading = [](std::string& str)
    {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos)
        {
            str = str.substr(startpos);
        }
    };

    std::istringstream iss(source);
    std::ostringstream oss;
    bool foundversion = false;
    //loop till we have a version then insert defines after
    for (std::string line; std::getline(iss, line);)
    {
        trimLeading(line);
        if(line.compare(0, 8, "#version") == 0)
        {
            oss << line << "\n";
            oss << defines;
        }
        else
        {
            oss << line << "\n";
        }
    }
    return oss.str();
}

// read a glsl file and inject defines based on shadermodemask and geometryatts
std::string osg2vsg::readGLSLShader(const std::string& filename, const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string sourceBuffer;
    if (!vsg::readFile(sourceBuffer, filename))
    {
        DEBUG_OUTPUT << "readGLSLShader: Failed to read file '" << filename << std::endl;
        return std::string();
    }

    std::string defines = createPSCDefinesString(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = insertDefinesInShaderSource(sourceBuffer, defines);
    return formatedSource;
}

// create a default vertex shader

std::string osg2vsg::createVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string defines = createPSCDefinesString(shaderModeMask, geometryAttrbutes);

    std::string source =
        "#version 450\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n";

    source += defines;

    source +=
        "layout(push_constant) uniform PushConstants {\n" \
        "    mat4 projection;\n" \
        "    mat4 view;\n" \
        "    mat4 model;\n" \
        "    //mat3 normal;\n" \
        "} pc; \n" \
        "layout(location = 0) in vec3 osg_Vertex;\n" \
        "#ifdef VSG_NORMAL\n" \
        "layout(location = 1) in vec3 osg_Normal;\n" \
        "layout(location = 1) out vec3 normalDir;\n" \
        "#endif\n" \
        "#ifdef VSG_TANGENT\n" \
        "layout(location = 2) in vec4 osg_Tangent;\n" \
        "#endif\n"
        "#ifdef VSG_COLOR\n" \
        "layout(location = 3) in vec4 osg_Color;\n" \
        "layout(location = 3) out vec4 vertColor;\n" \
        "#endif\n"
        "#ifdef VSG_TEXCOORD0\n" \
        "layout(location = 4) in vec2 osg_MultiTexCoord0;\n" \
        "layout(location = 4) out vec2 texCoord0;\n" \
        "#endif\n"
        "#ifdef VSG_LIGHTING\n" \
        "layout(location = 5) out vec3 viewDir;\n" \
        "layout(location = 6) out vec3 lightDir;\n" \
        "#endif\n" \
        "out gl_PerVertex{ vec4 gl_Position; };\n" \
        "\n" \
        "void main()\n" \
        "{\n" \
        "    gl_Position = (pc.projection * pc.view * pc.model) * vec4(osg_Vertex, 1.0);\n" \
        "#ifdef VSG_TEXCOORD0\n" \
        "    texCoord0 = osg_MultiTexCoord0.st;\n" \
        "#endif\n" \
        "#ifdef VSG_NORMAL\n" \
        "    vec3 n = ((pc.view * pc.model) * vec4(osg_Normal, 0.0)).xyz;\n" \
        "    normalDir = n;\n" \
        "#endif\n" \
        "#ifdef VSG_LIGHTING\n" \
        "    vec4 lpos = /*osg_LightSource.position*/ vec4(0.0, 0.25, 1.0, 0.0);\n" \
        "#ifdef VSG_NORMAL_MAP\n" \
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
        "#else\n" \
        "    viewDir = -vec3((pc.view * pc.model) * vec4(osg_Vertex, 1.0));\n" \
        "    if (lpos.w == 0.0)\n" \
        "        lightDir = lpos.xyz;\n" \
        "    else\n" \
        "        lightDir = lpos.xyz + viewDir;\n" \
        "#endif\n" \
        "#endif\n" \
        "#ifdef VSG_COLOR\n" \
        "    vertColor = osg_Color;\n" \
        "#endif\n" \
        "}\n";
    return source;
}

// create a default fragment shader

std::string osg2vsg::createFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string defines = createPSCDefinesString(shaderModeMask, geometryAttrbutes);

    std::string source =
        "#version 450\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n";

    source += defines;

    source +=
        "#ifdef VSG_DIFFUSE_MAP\n" \
        "layout(binding = 0) uniform sampler2D diffuseMap; \n" \
        "#endif\n" \
        "#ifdef VSG_OPACITY_MAP\n" \
        "layout(binding = 1) uniform sampler2D opacityMap;\n" \
        "#endif\n" \
        "#ifdef VSG_AMBIENT_MAP\n" \
        "layout(binding = 4) uniform sampler2D ambientMap; \n" \
        "#endif\n" \
        "#ifdef VSG_NORMAL_MAP\n" \
        "layout(binding = 5) uniform sampler2D normalMap;\n" \
        "#endif\n" \
        "#ifdef VSG_SPECULAR_MAP\n" \
        "layout(binding = 6) uniform sampler2D specularMap; \n" \
        "#endif\n" \

        "#ifdef VSG_NORMAL\n" \
        "layout(location = 1) in vec3 normalDir; \n" \
        "#endif\n" \
        "#ifdef VSG_COLOR\n" \
        "layout(location = 3) in vec4 vertColor; \n" \
        "#endif\n" \
        "#ifdef VSG_TEXCOORD0\n" \
        "layout(location = 4) in vec2 texCoord0;\n" \
        "#endif\n" \
        "#ifdef VSG_LIGHTING\n" \
        "layout(location = 5) in vec3 viewDir; \n" \
        "layout(location = 6) in vec3 lightDir;\n" \
        "#endif\n" \
        "layout(location = 0) out vec4 outColor;\n" \
        "\n" \
        "void main()\n" \
        "{\n" \
        "#ifdef VSG_DIFFUSE_MAP\n" \
        "    vec4 base = texture(diffuseMap, texCoord0.st);\n" \
        "#else\n" \
        "    vec4 base = vec4(1.0,1.0,1.0,1.0);\n" \
        "#endif\n" \
        "#ifdef VSG_COLOR\n" \
        "    base = base * vertColor;\n" \
        "#endif\n" \
        "#ifdef VSG_AMBIENT_MAP\n" \
        "    float ambientOcclusion = texture(ambientMap, texCoord0.st).r;\n" \
        "#else\n" \
        "    float ambientOcclusion = 1.0;\n" \
        "#endif\n" \
        "#ifdef VSG_SPECULAR_MAP\n" \
        "    vec3 specularColor = texture(specularMap, texCoord0.st).rgb;\n" \
        "#else\n" \
        "    vec3 specularColor = vec3(0.2,0.2,0.2);\n" \
        "#endif\n" \
        "#ifdef VSG_LIGHTING\n" \
        "#ifdef VSG_NORMAL_MAP\n" \
        "    vec3 nDir = texture(normalMap, texCoord0.st).xyz*2.0 - 1.0;\n" \
        "    nDir.g = -nDir.g;\n" \
        "#else\n" \
        "    vec3 nDir = normalDir;\n" \
        "#endif\n" \
        "    vec3 nd = normalize(nDir);\n" \
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
        "#else\n" \
        "    vec4 color = base;\n" \
        "#endif\n" \
        "    outColor = color;\n" \
        "#ifdef VSG_OPACITY_MAP\n" \
        "    outColor.a *= texture(opacityMap, texCoord0.st).r;\n" \
        "#endif\n" \
        "}\n";
    return source;
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
