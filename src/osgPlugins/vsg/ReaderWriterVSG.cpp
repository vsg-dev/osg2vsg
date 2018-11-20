// Released under the OSGPL license, as part of the OpenSceneGraph distribution.
//
// ReaderWriter for sgi's .rgb format.
// specification can be found at http://local.wasp.uwa.edu.au/~pbourke/dataformats/sgirgb/sgiversion.html

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

#include <vsg/io/AsciiInput.h>
#include <vsg/io/AsciiOutput.h>
#include <vsg/io/BinaryInput.h>
#include <vsg/io/BinaryOutput.h>
#include <vsg/io/FileSystem.h>

#include <vsg/nodes/Group.h>


class ReaderWriterVSG : public osgDB::ReaderWriter
{
    public:

        ReaderWriterVSG()
        {
            supportsExtension("vsga","vsg ascii format");
            supportsExtension("vsgb","vsg binary format");
        }

        virtual const char* className() const { return "VSG Reader/Writer"; }


        virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getLowerCaseFileExtension(file);
            if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

            std::string filename = osgDB::findDataFile( file, options );
            if (filename.empty()) return ReadResult::FILE_NOT_FOUND;

            vsg::ref_ptr<vsg::Object> object;
            try
            {
                if (ext=="vsga")
                {
                    std::ifstream fin(filename);
                    vsg::AsciiInput input(fin);
                    object = input.readObject("Root");
                }
                else if (ext=="vsga")
                {
                    std::ifstream fin(filename, std::ios::in | std::ios::binary);
                    vsg::BinaryInput input(fin);
                    object = input.readObject("Root");
                }
            }
            catch(std::string message)
            {
                OSG_NOTICE<<"Warning : attempt to read "<<filename<<" failed. mssage="<<message<<std::endl;
                return ReadResult::ERROR_IN_READING_FILE;
            }

            OSG_NOTICE<<"VSG data loaded "<<object->className()<<", need to implement a converter."<<std::endl;

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


        virtual WriteResult write(const vsg::Object* object, const std::string& filename, const osgDB::ReaderWriter::Options*) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            if (ext=="vsga")
            {
                std::ofstream fout(filename);
                vsg::AsciiOutput output(fout);
                output.writeObject("Root", object);
                return WriteResult::FILE_SAVED;
            }
            else if (ext=="vsgb")
            {
                std::ofstream fout(filename, std::ios::out | std::ios::binary);
                vsg::BinaryOutput output(fout);
                output.writeObject("Root", object);
                return WriteResult::FILE_SAVED;
            }

            return WriteResult::FILE_NOT_HANDLED;
        }

        virtual WriteResult writeImage(const osg::Image& image, const std::string& filename, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            vsg::ref_ptr<vsg::Data> data(new vsg::vec4Array2D(image.s(), image.t()));

            if (data) return write(data, filename, options);
            else return WriteResult::FILE_NOT_HANDLED;
        }

        virtual WriteResult writeNode(const osg::Node& node, const std::string& filename, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

            if (group) return write(group, filename, options);
            else return WriteResult::FILE_NOT_HANDLED;
        }

};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(vsg, ReaderWriterVSG)
