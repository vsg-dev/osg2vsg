// Released under the OSGPL license, as part of the OpenSceneGraph distribution.
//
// ReaderWriter for sgi's .rgb format.
// specification can be found at http://local.wasp.uwa.edu.au/~pbourke/dataformats/sgirgb/sgiversion.html

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

#include <osgUtil/Optimizer>
#include <osgUtil/MeshOptimizers>

#include <vsg/io/ReaderWriter.h>
#include <vsg/io/FileSystem.h>

#include <vsg/nodes/Group.h>

#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/ShaderUtils.h>
#include <osg2vsg/GeometryUtils.h>
#include <osg2vsg/SceneAnalysisVisitor.h>


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

            vsg::vsgReaderWriter io;
            vsg::ref_ptr<vsg::Object> object = io.readFile(filename);

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

            vsg::vsgReaderWriter io;
            if (io.writeFile(object, filename))
            {
                return WriteResult::FILE_SAVED;
            }

            return WriteResult::FILE_NOT_HANDLED;
        }

        virtual WriteResult writeImage(const osg::Image& image, const std::string& filename, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            //vsg::ref_ptr<vsg::Data> data(new vsg::vec4Array2D(image.s(), image.t()));
            auto data = osg2vsg::convertToVsg(&image);

            if (data) return write(data, filename, options);
            else return WriteResult::FILE_NOT_HANDLED;
        }

        virtual WriteResult writeNode(const osg::Node& node, const std::string& filename, const osgDB::ReaderWriter::Options* options) const
        {
            osg::Node& osg_scene = const_cast<osg::Node&>(node);

            std::string ext = osgDB::getFileExtension(filename);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            bool optimize = true;
            bool writeToFileProgramAndDataSetSets = false;
            vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

            if (optimize)
            {
                osgUtil::IndexMeshVisitor imv;
                #if OSG_MIN_VERSION_REQUIRED(3,6,4)
                imv.setGenerateNewIndicesOnAllGeometries(true);
                #endif
                osg_scene.accept(imv);
                imv.makeMesh();

                osgUtil::VertexCacheVisitor vcv;
                osg_scene.accept(vcv);
                vcv.optimizeVertices();

                osgUtil::VertexAccessOrderVisitor vaov;
                osg_scene.accept(vaov);
                vaov.optimizeOrder();

                osgUtil::Optimizer optimizer;
                optimizer.optimize(&osg_scene, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);
            }


            // Collect stats about the loaded scene
            osg2vsg::SceneAnalysisVisitor sceneAnalysis;
            sceneAnalysis.writeToFileProgramAndDataSetSets = writeToFileProgramAndDataSetSets;
            osg_scene.accept(sceneAnalysis);

            // build VSG scene
            vsg::ref_ptr<vsg::Node> converted_vsg_scene = sceneAnalysis.createVSG(searchPaths);

            if (converted_vsg_scene)
            {
                return write(converted_vsg_scene, filename, options);
            }
            return WriteResult::FILE_NOT_HANDLED;

        }

};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(vsg, ReaderWriterVSG)
