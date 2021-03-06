char defaultshader_frag[] = "#version 450\n"
                            "#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_DIFFUSE_MAP )\n"
                            "#extension GL_ARB_separate_shader_objects : enable\n"
                            "#ifdef VSG_DIFFUSE_MAP\n"
                            "layout(binding = 0) uniform sampler2D diffuseMap;\n"
                            "#endif\n"
                            "\n"
                            "#ifdef VSG_NORMAL\n"
                            "layout(location = 1) in vec3 normalDir;\n"
                            "#endif\n"
                            "#ifdef VSG_COLOR\n"
                            "layout(location = 3) in vec4 vertColor;\n"
                            "#endif\n"
                            "#ifdef VSG_TEXCOORD0\n"
                            "layout(location = 4) in vec2 texCoord0;\n"
                            "#endif\n"
                            "#ifdef VSG_LIGHTING\n"
                            "layout(location = 5) in vec3 viewDir;\n"
                            "layout(location = 6) in vec3 lightDir;\n"
                            "#endif\n"
                            "layout(location = 0) out vec4 outColor;\n"
                            "\n"
                            "void main()\n"
                            "{\n"
                            "#ifdef VSG_DIFFUSE_MAP\n"
                            "    vec4 base = texture(diffuseMap, texCoord0.st);\n"
                            "#else\n"
                            "    vec4 base = vec4(1.0,1.0,1.0,1.0);\n"
                            "#endif\n"
                            "#ifdef VSG_COLOR\n"
                            "    base = base * vertColor;\n"
                            "#endif\n"
                            "    float ambientOcclusion = 1.0;\n"
                            "    vec3 specularColor = vec3(0.2,0.2,0.2);\n"
                            "#ifdef VSG_LIGHTING\n"
                            "    vec3 nDir = normalDir;\n"
                            "    vec3 nd = normalize(nDir);\n"
                            "    vec3 ld = normalize(lightDir);\n"
                            "    vec3 vd = normalize(viewDir);\n"
                            "    vec4 color = vec4(0.01, 0.01, 0.01, 1.0);\n"
                            "    color += /*osg_Material.ambient*/ vec4(0.1, 0.1, 0.1, 0.0);\n"
                            "    float diff = max(dot(ld, nd), 0.0);\n"
                            "    color += /*osg_Material.diffuse*/ vec4(0.8, 0.8, 0.8, 0.0) * diff;\n"
                            "    color *= ambientOcclusion;\n"
                            "    color *= base;\n"
                            "    if (diff > 0.0)\n"
                            "    {\n"
                            "        vec3 halfDir = normalize(ld + vd);\n"
                            "        color.rgb += base.a * specularColor *\n"
                            "            pow(max(dot(halfDir, nd), 0.0), 16.0/*osg_Material.shininess*/);\n"
                            "    }\n"
                            "#else\n"
                            "    vec4 color = base;\n"
                            "#endif\n"
                            "    outColor = color;\n"
                            "    if (outColor.a==0.0) discard;\n"
                            "}\n"
                            "\n"
                            "\n";
