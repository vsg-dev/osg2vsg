#include <osg2vsg/ShaderUtils.h>

#include <osg2vsg/GeometryUtils.h>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "glsllang/ResourceLimits.h"

#include <algorithm>
#include <iomanip>

using namespace osg2vsg;

#if 1
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif
#define INFO_OUTPUT std::cout


uint32_t osg2vsg::calculateShaderModeMask(const osg::StateSet* stateSet)
{
    uint32_t stateMask = 0;
    if (stateSet)
    {
        if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) stateMask |= BLEND;
        if (stateSet->getMode(GL_LIGHTING) & osg::StateAttribute::ON)  stateMask |= LIGHTING;

        auto asMaterial = dynamic_cast<const osg::Material*>(stateSet->getAttribute(osg::StateAttribute::Type::MATERIAL));
        if (asMaterial && stateSet->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON) stateMask |= MATERIAL;

        auto hasTextureWithImageInChannel = [stateSet](unsigned int channel)
        {
            auto asTex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(channel, osg::StateAttribute::TEXTURE));
            if (asTex && asTex->getImage(0)) return true;
            return false;
        };

        if (hasTextureWithImageInChannel(DIFFUSE_TEXTURE_UNIT)) stateMask |= DIFFUSE_MAP;
        if (hasTextureWithImageInChannel(OPACITY_TEXTURE_UNIT)) stateMask |= OPACITY_MAP;
        if (hasTextureWithImageInChannel(AMBIENT_TEXTURE_UNIT)) stateMask |= AMBIENT_MAP;
        if (hasTextureWithImageInChannel(NORMAL_TEXTURE_UNIT)) stateMask |= NORMAL_MAP;
        if (hasTextureWithImageInChannel(SPECULAR_TEXTURE_UNIT)) stateMask |= SPECULAR_MAP;
    }
    return stateMask;
}

// create defines string based of shader mask

static std::vector<std::string> createPSCDefineStrings(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hastanget = geometryAttrbutes & TANGENT;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;

    std::vector<std::string> defines;

    // vertx inputs
    if (hasnormal) defines.push_back("VSG_NORMAL");
    if (hascolor) defines.push_back("VSG_COLOR");
    if (hastex0) defines.push_back("VSG_TEXCOORD0");
    if (hastanget) defines.push_back("VSG_TANGENT");

    // shading modes/maps
    if (hasnormal && (shaderModeMask & LIGHTING)) defines.push_back("VSG_LIGHTING");
    
    if(shaderModeMask & MATERIAL) defines.push_back("VSG_MATERIAL");

    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines.push_back("VSG_DIFFUSE_MAP");
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines.push_back("VSG_OPACITY_MAP");
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines.push_back("VSG_AMBIENT_MAP");
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines.push_back("VSG_NORMAL_MAP");
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines.push_back("VSG_SPECULAR_MAP");

    if (shaderModeMask & BILLBOARD) defines.push_back("VSG_BILLBOARD");

    if (shaderModeMask & SHADER_TRANSLATE) defines.push_back("VSG_TRANSLATE");

    return defines;
}

// insert defines string after the version in source

static std::string processGLSLShaderSource(const std::string& source, const std::vector<std::string>& defines)
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

static std::string debugFormatShaderSource(const std::string& source)
{
    std::istringstream iss(source);
    std::ostringstream oss;

    uint32_t linecount = 1;

    for (std::string line; std::getline(iss, line);)
    {
        oss << std::setw(4) << std::setfill(' ') << linecount << ": " << line << "\n";
        linecount++;
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

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(sourceBuffer, defines);
    return formatedSource;
}

// create an fbx vertex shader
#include "shaders/fbxshader_vert.cpp"

std::string osg2vsg::createFbxVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(fbxshader_vert, defines);

    return formatedSource;
}

// create an fbx fragment shader
#include "shaders/fbxshader_frag.cpp"

std::string osg2vsg::createFbxFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(fbxshader_frag, defines);

    return formatedSource;
}

// create a default vertex shader
#include "shaders/defaultshader_vert.cpp"

std::string osg2vsg::createDefaultVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(defaultshader_vert, defines);

    return formatedSource;
}

// create a default fragment shader
#include "shaders/defaultshader_frag.cpp"

std::string osg2vsg::createDefaultFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(defaultshader_frag, defines);

    return formatedSource;
}

ShaderCompiler::ShaderCompiler(vsg::Allocator* allocator):
    Inherit(allocator)
{
    glslang::InitializeProcess();
}

ShaderCompiler::~ShaderCompiler()
{
    glslang::FinalizeProcess();
}

bool ShaderCompiler::compile(vsg::ShaderStages& shaders)
{
    auto getFriendlyNameForShader = [](const vsg::ref_ptr<vsg::ShaderStage>& vsg_shader)
    {
        switch (vsg_shader->getShaderStageFlagBits())
        {
            case(VK_SHADER_STAGE_VERTEX_BIT): return "Vertex Shader";
            case(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): return "Tessellation Control Shader";
            case(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): return "Tessellation Evaluation Shader";
            case(VK_SHADER_STAGE_GEOMETRY_BIT): return "Geometry Shader";
            case(VK_SHADER_STAGE_FRAGMENT_BIT): return "Fragment Shader";
            case(VK_SHADER_STAGE_COMPUTE_BIT): return "Compute Shader";
            default: return "Unkown Shader Type";
        }
        return "";
    };

    using StageShaderMap = std::map<EShLanguage, vsg::ref_ptr<vsg::ShaderStage>>;
    using TShaders = std::list<std::unique_ptr<glslang::TShader>>;
    TShaders tshaders;

    TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;

    StageShaderMap stageShaderMap;
    std::unique_ptr<glslang::TProgram> program(new glslang::TProgram);

    for(auto& vsg_shader : shaders)
    {
        EShLanguage envStage = EShLangCount;
        switch(vsg_shader->getShaderStageFlagBits())
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

        const char* str = vsg_shader->getShaderModule()->source().c_str();
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
            // print error infomation
            INFO_OUTPUT << std::endl << "----  " << getFriendlyNameForShader(vsg_shader) << "  ----" << std::endl << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
            INFO_OUTPUT << "Warning: GLSL source failed to parse." << std::endl;
            INFO_OUTPUT << "glslang info log: " << std::endl << shader->getInfoLog();
            DEBUG_OUTPUT << "glslang debug info log: " << std::endl << shader->getInfoDebugLog();
            INFO_OUTPUT << "--------" << std::endl;
        }
    }

    if (stageShaderMap.empty() || stageShaderMap.size() != shaders.size())
    {
        DEBUG_OUTPUT<<"ShaderCompiler::compile(Shaders& shaders) stageShaderMap.size() != shaders.size()"<<std::endl;
        return false;
    }

    EShMessages messages = EShMsgDefault;
    if (!program->link(messages))
    {
        INFO_OUTPUT << std::endl << "----  Program  ----" << std::endl << std::endl;

        for (auto& vsg_shader : shaders)
        {
            INFO_OUTPUT << std::endl << getFriendlyNameForShader(vsg_shader) << ":" << std::endl << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
        }

        INFO_OUTPUT << "Warning: Program failed to link." << std::endl;
        INFO_OUTPUT << "glslang info log: " << std::endl << program->getInfoLog();
        DEBUG_OUTPUT << "glslang debug info log: " << std::endl << program->getInfoDebugLog();
        INFO_OUTPUT << "--------" << std::endl;

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
            glslang::GlslangToSpv(*(program->getIntermediate((EShLanguage)eshl_stage)), vsg_shader->getShaderModule()->spirv(), &logger, &spvOptions);
        }
    }

    return true;
}
