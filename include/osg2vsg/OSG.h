#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <osg2vsg/Export.h>

namespace osg2vsg
{
    // forward declare
    struct PipelineCache;

    /// optional OSG ReaderWriter
    class OSG2VSG_DECLSPEC OSG : public vsg::Inherit<vsg::ReaderWriter, OSG>
    {
    public:
        OSG();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const override;

        bool getFeatures(Features& features) const override;

        // vsg::Options::setValue(str, value) supported options:
        static constexpr const char* original_converter = "original_converter";   // select early osg2vsg implementation
        static constexpr const char* read_build_options = "read_build_options";   // read build options from specified file
        static constexpr const char* write_build_options = "write_build_options"; // write build options to specified file

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

    protected:

        vsg::ref_ptr<PipelineCache> pipelineCache;

        ~OSG();
    };

} // namespace vsgXchange

EVSG_type_name(osg2vsg::OSG);
