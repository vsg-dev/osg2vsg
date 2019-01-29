#include <osg2vsg/GeometryUtils.h>

namespace osg2vsg
{

    vsg::ref_ptr<vsg::vec2Array> convertToVsg(osg::Vec2Array* inarray)
    {
        vsg::ref_ptr<vsg::vec2Array> outarray(new vsg::vec2Array(inarray->size()));
        for (unsigned int i = 0; i < inarray->size(); i++)
        {
            osg::Vec2 osg2 = inarray->at(i);
            vsg::vec2 vsg2(osg2.x(), osg2.y());
            outarray->set(i, vsg2);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::vec3Array> convertToVsg(osg::Vec3Array* inarray)
    {
        vsg::ref_ptr<vsg::vec3Array> outarray(new vsg::vec3Array(inarray->size()));
        for (unsigned int i = 0; i < inarray->size(); i++)
        {
            osg::Vec3 osg3 = inarray->at(i);
            vsg::vec3 vsg3(osg3.x(), osg3.y(), osg3.z());
            outarray->set(i, vsg3);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::vec4Array> convertToVsg(osg::Vec4Array* inarray)
    {
        vsg::ref_ptr<vsg::vec4Array> outarray(new vsg::vec4Array(inarray->size()));
        for (unsigned int i = 0; i < inarray->size(); i++)
        {
            osg::Vec4 osg4 = inarray->at(i);
            vsg::vec4 vsg4(osg4.x(), osg4.y(), osg4.z(), osg4.w());
            outarray->set(i, vsg4);
        }
        return outarray;
    }

    vsg::ref_ptr<vsg::Data> convertToVsg(osg::Array* inarray)
    {
        switch (inarray->getType())
        {
            case osg::Array::Type::Vec2ArrayType: return convertToVsg(dynamic_cast<osg::Vec2Array*>(inarray));
            case osg::Array::Type::Vec3ArrayType: return convertToVsg(dynamic_cast<osg::Vec3Array*>(inarray));
            case osg::Array::Type::Vec4ArrayType: return convertToVsg(dynamic_cast<osg::Vec4Array*>(inarray));
            default: return vsg::ref_ptr<vsg::Data>();
        }
    }

    vsg::ref_ptr<vsg::Geometry> convertToVsg(osg::Geometry* ingeometry)
    {
        // convert attribute arrays
        vsg::ref_ptr<vsg::Data> vertices(osg2vsg::convertToVsg(ingeometry->getVertexArray()));
        vsg::ref_ptr<vsg::Data> normals(osg2vsg::convertToVsg(ingeometry->getNormalArray()));
        vsg::ref_ptr<vsg::Data> colors(osg2vsg::convertToVsg(ingeometry->getColorArray()));
        vsg::ref_ptr<vsg::Data> texcoords(osg2vsg::convertToVsg(ingeometry->getTexCoordArray(0)));

        // convert indicies
        osg::Geometry::DrawElementsList drawElementsList;
        ingeometry->getDrawElementsList(drawElementsList);

        // only support first for now
        if (drawElementsList.size() == 0) return vsg::ref_ptr<vsg::Geometry>();

        osg::DrawElements* osgindices = drawElementsList.at(0);
        unsigned int numindcies = osgindices->getNumIndices();

        vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray(numindcies));
        for (unsigned int i = 0; i < numindcies; i++)
        {
            indices->set(i, osgindices->index(i));
        }

        // create the vsg geometry
        auto geometry = vsg::Geometry::create();

        geometry->_arrays = vsg::DataList{ vertices, normals, colors, texcoords };
        geometry->_indices = indices;

        vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(indices->valueCount(), 1, 0, 0, 0);
        geometry->_commands = vsg::Geometry::Commands{ drawIndexed };

        return geometry;
    }
}


