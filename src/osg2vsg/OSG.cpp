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

#include <vsg/io/Logger.h>
#include <osg2vsg/OSG.h>

using namespace osg2vsg;

OSG::OSG()
{
    vsg::info("OSG::OSG()");
}

OSG::~OSG()
{
    vsg::info("OSG::~OSG()");
}

vsg::ref_ptr<vsg::Object> OSG::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    vsg::info("OSG::read(", filename, ", ", options,")");
    return {};
}

bool OSG::getFeatures(Features& features) const
{
    vsg::info("OSG::getFeatures(..)");
    return false;
}

bool OSG::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    vsg::info("OSG::readOptions(..)");
    return false;
}
