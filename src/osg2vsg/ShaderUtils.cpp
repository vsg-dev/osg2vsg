#include <osg2vsg/ShaderUtils.h>

#include <osg2vsg/GeometryUtils.h>
#include <shaderc/shaderc.h>

namespace osg2vsg
{
    uint32_t calculateStateMask(osg::StateSet* stateSet)
    {
        uint32_t stateMask = 0;
        //if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON)
        //    stateMask |= ShaderGen::BLEND;
        if (stateSet->getMode(GL_LIGHTING) & osg::StateAttribute::ON)
            stateMask |= LIGHTING;
        if (stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
            stateMask |= DIFFUSE_MAP;
        if (stateSet->getTextureAttribute(1, osg::StateAttribute::TEXTURE) /*&& geometry != 0 && geometry->getVertexAttribArray(6)*/)
            stateMask |= NORMAL_MAP;
        return stateMask;
    }

    // create vertex shader source using statemask to determine type of shader to build and geometryattributes to determine attribute binding locations

    std::string createVertexSource(const uint32_t& stateMask, const uint32_t& geometryAttrbutes, bool osgCompatible)
    {
        bool hasnormal = geometryAttrbutes & NORMAL;
        bool hascolor = geometryAttrbutes & COLOR;
        bool hastex0 = geometryAttrbutes & TEXCOORD0;

        // regardless of if we use the attribute figure out what it's index should be
        uint32_t inputindex = 0;

        uint32_t vertexindex = osgCompatible ? 0 : (inputindex++);
        uint32_t normalindex = osgCompatible ? 1 : (hasnormal ? inputindex++ : 0);
        uint32_t colorindex = osgCompatible ? 2 : (hascolor ? inputindex++ : 0);
        uint32_t tex0index = osgCompatible ? 3 : (hastex0 ? inputindex++ : 0);

        bool usenormal = hasnormal && (stateMask & (LIGHTING | NORMAL_MAP));
        bool usetex0 = hastex0 && (stateMask & (DIFFUSE_MAP | NORMAL_MAP));
        bool uselighting = usenormal && (stateMask & (LIGHTING));
        bool usediffusemap = usetex0 && (stateMask & (DIFFUSE_MAP));
        bool usenormalmap = usetex0 && (stateMask & (NORMAL_MAP));

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
            if (usenormal) uniforms << "uniform mat3 osg_NormalMatrix;\n";

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

        if (usenormal) inputs << "layout(location = " << normalindex << ") in vec3 osg_Normal;\n";

        if (usetex0) inputs << "layout(location = " << tex0index << ") in vec2 osg_MultiTexCoord0;\n";

        if (usenormalmap)
        {
            inputs << "layout(location = 6) in vec3 tangent;\n"; // figure this out later
        }

        // vert outputs

        if (usetex0)
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

        if (usetex0)
        {
            vert << "  texCoord0 = osg_MultiTexCoord0.st;\n";
        }

        // for now hard code the light position
        if(uselighting || usenormalmap) vert << "  vec4 lpos = /*osg_LightSource.position*/ vec4(0.0,100.0,0.0,0.0);\n";

        if (usenormalmap)
        {
            vert <<
                "  vec3 n = " << nmat << " * osg_Normal;\n"\
                "  vec3 t = " << nmat << " * tangent;\n"\
                "  vec3 b = cross(n, t);\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir.x = dot(dir, t);\n"\
                "  viewDir.y = dot(dir, b);\n"\
                "  viewDir.z = dot(dir, n);\n";

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
            vert <<
                "  normalDir = " << nmat << " * osg_Normal;\n"\
                "  vec3 dir = -vec3(" << mvmat << " * vec4(osg_Vertex, 1.0));\n"\
                "  viewDir = dir;\n";

            vert <<
                "  if (lpos.w == 0.0)\n"\
                "    lightDir = lpos.xyz;\n"\
                "  else\n"\
                "    lightDir = lpos.xyz + dir;\n";
        }

        vert << "}\n";

        return vert.str();
    }

    std::string createFragmentSource(const uint32_t& stateMask, const uint32_t& geometryAttrbutes, bool osgCompatible)
    {
        bool hasnormal = geometryAttrbutes & NORMAL;
        bool hascolor = geometryAttrbutes & COLOR;
        bool hastex0 = geometryAttrbutes & TEXCOORD0;

        bool usenormal = hasnormal && (stateMask & (LIGHTING | NORMAL_MAP));
        bool usetex0 = hastex0 && (stateMask & (DIFFUSE_MAP | NORMAL_MAP));
        bool uselighting = usenormal && (stateMask & (LIGHTING));
        bool usediffusemap = usetex0 && (stateMask & (DIFFUSE_MAP));
        bool usenormalmap = usetex0 && (stateMask & (NORMAL_MAP));

        std::ostringstream uniforms;
        std::ostringstream inputs;
        std::ostringstream outputs;

        // inputs

        if (usetex0)
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

        if (usediffusemap)
        {
            uniforms << "layout(binding = 0) uniform sampler2D diffuseMap;\n";
        }

        if (usenormalmap)
        {
            uniforms << "layout(binding = 1) uniform sampler2D normalMap;\n";
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

        if (usenormalmap)
        {
            frag << "  vec3 normalDir = texture(normalMap, texCoord0.st).xyz*2.0-1.0;\n";
            //frag << " normalDir.g = -normalDir.g;\n";
        }

        if (uselighting || usenormalmap)
        {
            frag <<
                "  vec3 nd = normalize(normalDir);\n"\
                "  vec3 ld = normalize(lightDir);\n"\
                "  vec3 vd = normalize(viewDir);\n"\
                "  vec4 color = vec4(0.01,0.01,0.01,1.0);\n"\
                "  color += /*osg_Material.ambient*/ vec4(0.1,0.1,0.1,0.0);\n"\
                "  float diff = max(dot(ld, nd), 0.0);\n"\
                "  color += /*osg_Material.diffuse*/ vec4(0.8,0.8,0.8,0.0) * diff;\n"\
                "  color *= base;\n"\
                "  if (diff > 0.0)\n"\
                "  {\n"\
                "    vec3 halfDir = normalize(ld+vd);\n"\
                "    color.rgb += base.a * /*osg_Material.specular.rgb*/ vec3(0.2,0.2,0.2) * \n"\
                "      pow(max(dot(halfDir, nd), 0.0), /*osg_Material.shine*/ 16.0);\n"\
                "  }\n";
        }
        else
        {
            frag << "  vec4 color = base;\n";
        }

        frag << "  outColor = color;\n";
        frag << "}\n";

        return frag.str();
    }

    vsg::ref_ptr<vsg::Shader> compileSourceToSPV(const std::string& source, bool isvert)
    {
        shaderc_compiler_t compiler = shaderc_compiler_initialize();
        shaderc_compile_options_t options = shaderc_compile_options_initialize();

        shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, source.c_str(), source.length(),
                                                                        (isvert ? shaderc_glsl_vertex_shader : shaderc_glsl_fragment_shader),
                                                                        (isvert ? "vert shader" : "frag shader"), "main", options);

        // check the status
        shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

        if (status != shaderc_compilation_status_success)
        {
            // failed, print errors
            const char* errorstr = shaderc_result_get_error_message(result);

            std::cout << "Error compiling shader source:" << std::endl;
            std::cout << source << std::endl << std::endl;
            std::cout << "Returned status " << status << " and the following errors..." << std::endl << errorstr << std::endl;
            return vsg::ref_ptr<vsg::Shader>();
        }

        // get the binary info
        size_t byteslength = shaderc_result_get_length(result);
        const char* bytes = shaderc_result_get_bytes(result);

        std::string compiledsource = std::string(bytes, byteslength);

        // pad to multiple of 4
        const int padding = 4 - (compiledsource.length() % 4);
        if (padding < 4) {
            for (int i = 0; i < padding; ++i) {
                compiledsource += ' ';
            }
        }
        
        std::cout << "Shader compilation succeeded, returned " << byteslength << " bytes." << std::endl;

        // copy the binary into a shader content buffer and use it to create vsg shader
        size_t contentValueSize = sizeof(uint32_t);
        size_t contentBufferSize = compiledsource.size() / contentValueSize;

        vsg::Shader::Contents shadercontents(contentBufferSize);
        memcpy(shadercontents.data(), compiledsource.c_str(), compiledsource.size());

        vsg::ref_ptr<vsg::Shader> shader = vsg::Shader::create(isvert ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, "main", shadercontents);

        // release the result
        shaderc_result_release(result);

        return shader;
    }
}


