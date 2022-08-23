#include <vsg/io/VSG.h>
static auto defaultshader_vert = []() {std::istringstream str(
R"(#vsga 0.5.4
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 1
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING )
#extension GL_ARB_separate_shader_objects : enable
layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 moddelview;
    //mat3 normal;
} pc;
layout(location = 0) in vec3 osg_Vertex;
#ifdef VSG_NORMAL
layout(location = 1) in vec3 osg_Normal;
layout(location = 1) out vec3 normalDir;
#endif
#ifdef VSG_COLOR
layout(location = 3) in vec4 osg_Color;
layout(location = 3) out vec4 vertColor;
#endif\n
#ifdef VSG_TEXCOORD0
layout(location = 4) in vec2 osg_MultiTexCoord0;
layout(location = 4) out vec2 texCoord0;
#endif\n
#ifdef VSG_LIGHTING
layout(location = 5) out vec3 viewDir;
layout(location = 6) out vec3 lightDir;
#endif
out gl_PerVertex{ vec4 gl_Position; };

void main()
{
    gl_Position = (pc.projection * pc.modelview) * vec4(osg_Vertex, 1.0);
#ifdef VSG_TEXCOORD0
    texCoord0 = osg_MultiTexCoord0.st;
#endif
#ifdef VSG_NORMAL
    vec3 n = ((pc.mdoelview) * vec4(osg_Normal, 0.0)).xyz;
    normalDir = n;
#endif
#ifdef VSG_LIGHTING
    vec4 lpos = /*osg_LightSource.position*/ vec4(0.0, 0.25, 1.0, 0.0);
    viewDir = -vec3((pc.modelview) * vec4(osg_Vertex, 1.0));
    if (lpos.w == 0.0)
        lightDir = lpos.xyz;
    else
        lightDir = lpos.xyz + viewDir;
#endif
#ifdef VSG_COLOR
    vertColor = osg_Color;
#endif
}
"
    code 0
    
  }
  NumSpecializationConstants 0
}
)");
vsg::VSG io;
return io.read_cast<vsg::ShaderStage>(str);
};
