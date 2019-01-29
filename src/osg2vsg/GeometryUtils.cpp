#include <osg2vsg/GeometryUtils.h>

namespace osg2vsg
{

    vsg::vec2Array* convert(osg::Vec2Array* inarray)
    {
        vsg::vec2Array* outarray(new vsg::vec2Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec2 osg2 = inarray->at(i);
            vsg::vec2 vsg2(osg2.x(), osg2.y());
            outarray->set(i, vsg2);
        }
        return outarray;
    }

    vsg::vec3Array* convert(osg::Vec3Array* inarray)
    {
        vsg::vec3Array* outarray(new vsg::vec3Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec3 osg3 = inarray->at(i);
            vsg::vec3 vsg3(osg3.x(), osg3.y(), osg3.z());
            outarray->set(i, vsg3);
        }
        return outarray;
    }

    vsg::vec4Array* convert(osg::Vec4Array* inarray)
    {
        vsg::vec4Array* outarray(new vsg::vec4Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec4 osg4 = inarray->at(i);
            vsg::vec4 vsg4(osg4.x(), osg4.y(), osg4.z(), osg4.w());
            outarray->set(i, vsg4);
        }
        return outarray;
    }

    vsg::Data* convert(osg::Array* inarray)
    {
        switch (inarray->getType())
        {
            case osg::Array::Type::Vec2ArrayType: return convert(dynamic_cast<osg::Vec2Array*>(inarray));
            case osg::Array::Type::Vec3ArrayType: return convert(dynamic_cast<osg::Vec3Array*>(inarray));
            case osg::Array::Type::Vec4ArrayType: return convert(dynamic_cast<osg::Vec4Array*>(inarray));
            default: return nullptr;
        }
    }
}


