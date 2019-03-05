#include <osg2vsg/ShaderUtils.h>

#include <osg2vsg/GeometryUtils.h>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "glsllang/ResourceLimits.h"

#include <algorithm>
#include <iomanip>

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

std::vector<std::string> createPSCDefineStrings(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;
    bool hastanget = geometryAttrbutes & TANGENT;

    std::vector<std::string> defines;

    // vertx inputs
    if (hasnormal) defines.push_back("VSG_NORMAL");
    if (hascolor) defines.push_back("VSG_COLOR");
    if (hastex0) defines.push_back("VSG_TEXCOORD0");
    if (hastanget) defines.push_back("VSG_TANGENT");

    // shading modes/maps
    if (hasnormal && (shaderModeMask & LIGHTING)) defines.push_back("VSG_LIGHTING");
    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines.push_back("VSG_DIFFUSE_MAP");
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines.push_back("VSG_OPACITY_MAP");
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines.push_back("VSG_AMBIENT_MAP");
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines.push_back("VSG_NORMAL_MAP");
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines.push_back("VSG_SPECULAR_MAP");

    return defines;
}

// insert defines string after the version in source

std::string processGLSLShaderSource(const std::string& source, const std::vector<std::string>& defines)
{
    // trim leading spaces/tabs
    auto trimLeading = [](std::string& str)
    {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos)
        {
            str = str.substr(startpos);
        }
    };

    // trim trailing spaces/tabs/newlines
    auto trimTrailing = [](std::string& str)
    {
        size_t endpos = str.find_last_not_of(" \t\n");
        if (endpos != std::string::npos)
        {
            str = str.substr(0, endpos+1);
        }
    };

    // sanitise line by triming leading and trailing characters
    auto sanitise = [&trimLeading, &trimTrailing](std::string& str)
    {
        trimLeading(str);
        trimTrailing(str);
    };

    // return true if str starts with match string
    auto startsWith = [](const std::string& str, const std::string& match)
    {
        return str.compare(0, match.length(), match) == 0;
    };

    // returns the string between the start and end character
    auto stringBetween = [](const std::string& str, const char& startChar, const char& endChar)
    {
        auto start = str.find_first_of(startChar);
        if (start == std::string::npos) return std::string();

        auto end = str.find_first_of(endChar, start);
        if (end == std::string::npos) return std::string();

        if((end - start) - 1 == 0) return std::string();

        return str.substr(start+1, (end - start) - 1);
    };

    auto split = [](const std::string& str, const char& seperator)
    {
        std::vector<std::string> elements;

        std::string::size_type prev_pos = 0, pos = 0;

        while ((pos = str.find(seperator, pos)) != std::string::npos)
        {
            auto substring = str.substr(prev_pos, pos - prev_pos);
            elements.push_back(substring);
            prev_pos = ++pos;
        }

        elements.push_back(str.substr(prev_pos, pos - prev_pos));

        return elements;
    };

    auto addLine = [](std::ostringstream& ss, const std::string& line)
    {
        ss << line << "\n";
    };

    std::istringstream iss(source);
    std::ostringstream headerstream;
    std::ostringstream sourcestream;
    bool foundversion = false;

    const std::string versionmatch = "#version";
    const std::string importdefinesmatch = "#pragma import_defines";

    std::vector<std::string> finaldefines;

    for (std::string line; std::getline(iss, line);)
    {
        std::string sanitisedline = line;
        sanitise(sanitisedline);

        // is it the version
        if(startsWith(sanitisedline, versionmatch))
        {
            addLine(headerstream, line);
        }
        // is it the defines import
        else if (startsWith(sanitisedline, importdefinesmatch))
        {
            // get the import defines between ()
            auto csv = stringBetween(sanitisedline, '(', ')');
            auto importedDefines = split(csv, ',');

            addLine(headerstream, line);

            // loop the imported defines and see if it's also requested in defines, if so insert a define line
            for (auto importedDef : importedDefines)
            {
                auto sanitiesedImportDef = importedDef;
                sanitise(sanitiesedImportDef);

                auto finditr = std::find(defines.begin(), defines.end(), sanitiesedImportDef);
                if (finditr != defines.end())
                {
                    addLine(headerstream, "#define " + sanitiesedImportDef);
                }
            }
        }
        else
        {
            // standard source line
            addLine(sourcestream, line);
        }
    }

    return headerstream.str() + sourcestream.str();
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

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(sourceBuffer, defines);
    return formatedSource;
}

// create a default vertex shader

std::string osg2vsg::createVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string source =
        "#version 450\n" \
        "#pragma import_defines ( VSG_NORMAL, VSG_TANGENT, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_NORMAL_MAP )\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n" \
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

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(source, defines);

    return formatedSource;
}

// create a default fragment shader

std::string osg2vsg::createFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string source =
        "#version 450\n" \
        "#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_DIFFUSE_MAP, VSG_OPACITY_MAP, VSG_AMBIENT_MAP, VSG_NORMAL_MAP, VSG_SPECULAR_MAP )\n" \
        "#extension GL_ARB_separate_shader_objects : enable\n" \
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

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(source, defines);

    return formatedSource;
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

bool ShaderCompiler::compile(vsg::ShaderModules& shaders)
{
    using StageShaderMap = std::map<EShLanguage, vsg::ref_ptr<vsg::ShaderModule>>;
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
            vsg::ShaderModule::SPIRV spirv;
            std::string warningsErrors;
            spv::SpvBuildLogger logger;
            glslang::SpvOptions spvOptions;
            glslang::GlslangToSpv(*(program->getIntermediate((EShLanguage)eshl_stage)), vsg_shader->spirv(), &logger, &spvOptions);
        }
    }

    return true;
}
