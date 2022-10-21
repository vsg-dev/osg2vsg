/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "ShaderUtils.h"
#include "GeometryUtils.h"

#include <algorithm>
#include <iomanip>

using namespace osg2vsg;

uint32_t osg2vsg::calculateShaderModeMask(const osg::StateSet* stateSet)
{
    uint32_t stateMask = 0;
    if (stateSet)
    {
        if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) stateMask |= BLEND;
        if (stateSet->getMode(GL_LIGHTING) & osg::StateAttribute::ON) stateMask |= LIGHTING;

        auto asMaterial = dynamic_cast<const osg::Material*>(stateSet->getAttribute(osg::StateAttribute::Type::MATERIAL));
        if (asMaterial && stateSet->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON) stateMask |= MATERIAL;

        auto hasTextureWithImageInChannel = [stateSet](unsigned int channel) {
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

std::set<std::string> osg2vsg::createPSCDefineStrings(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hastanget = geometryAttrbutes & TANGENT;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;

    std::set<std::string> defines;

    // vertx inputs
    if (hasnormal) defines.insert("VSG_NORMAL");
    if (hascolor) defines.insert("VSG_COLOR");
    if (hastex0) defines.insert("VSG_TEXCOORD0");
    if (hastanget) defines.insert("VSG_TANGENT");

    // shading modes/maps
    if (hasnormal && (shaderModeMask & LIGHTING)) defines.insert("VSG_LIGHTING");

    if (shaderModeMask & MATERIAL) defines.insert("VSG_MATERIAL");

    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines.insert("VSG_DIFFUSE_MAP");
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines.insert("VSG_OPACITY_MAP");
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines.insert("VSG_AMBIENT_MAP");
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines.insert("VSG_NORMAL_MAP");
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines.insert("VSG_SPECULAR_MAP");

    if (shaderModeMask & BILLBOARD) defines.insert("VSG_BILLBOARD");

    if (shaderModeMask & SHADER_TRANSLATE) defines.insert("VSG_TRANSLATE");

    return defines;
}
