// Released under the OSGPL license, as part of the OpenSceneGraph distribution.
//
// ReaderWriter for converting and writing scenes to vsgt/vsgb files. Reading files back in is not currently implemented.

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

#include <vsg/io/Logger.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/core/Data.h>
#include <vsg/nodes/Node.h>

namespace osg2vsg
{
    vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Image* osg_image)
    {
        vsg::info("convertToVsg(", osg_image, ")");
        return {};
    }

    vsg::ref_ptr<vsg::Node> convertToVsg(const osg::Node* osg_node)
    {
        vsg::info("convertToVsg(", osg_node, ")");
        return {};
    }
}


class ReaderWriterVSG : public osgDB::ReaderWriter
{
    public:

        ReaderWriterVSG()
        {
            supportsExtension("vsga","vsg ascii format");
            supportsExtension("vsgt","vsg ascii format");
            supportsExtension("vsgb","vsg binary format");
        }

        virtual const char* className() const { return "VSG Reader/Writer"; }


        virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getLowerCaseFileExtension(file);
            if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

            std::string filename = osgDB::findDataFile( file, options );
            if (filename.empty()) return ReadResult::FILE_NOT_FOUND;

            auto object = vsg::read(filename);

            OSG_NOTICE<<"VSG data loaded "<<object<<", need to implement a converter."<<std::endl;

            return ReadResult::FILE_NOT_HANDLED;
        }

        virtual ReadResult readImage(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            ReadResult result = readObject(file, options);
            if (!result.success()) return result;
            if (result.validImage()) return result.getImage();
            return ReadResult::FILE_NOT_HANDLED;
        }

        virtual ReadResult readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            ReadResult result = readObject(file, options);
            if (!result.success()) return result;
            if (result.validNode()) return result.getNode();
            return ReadResult::FILE_NOT_HANDLED;
        }


        virtual WriteResult writeImage(const osg::Image& image, const std::string& filename, const osgDB::ReaderWriter::Options* /*options*/) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            auto vsg_data = osg2vsg::convertToVsg(&image);
            vsg::ref_ptr<vsg::Options> vsg_options;

            if (vsg_data) return vsg::write(vsg_data, filename, vsg_options) ? WriteResult::FILE_SAVED : WriteResult::FILE_NOT_HANDLED;
            else return WriteResult::FILE_NOT_HANDLED;
        }

        virtual WriteResult writeNode(const osg::Node& node, const std::string& filename, const osgDB::ReaderWriter::Options* /*options*/) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            auto vsg_node = osg2vsg::convertToVsg(&node);
            vsg::ref_ptr<vsg::Options> vsg_options;

            if (vsg_node) return vsg::write(vsg_node, filename, vsg_options) ? WriteResult::FILE_SAVED : WriteResult::FILE_NOT_HANDLED;
            else return WriteResult::FILE_NOT_HANDLED;
        }

};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(vsg, ReaderWriterVSG)
