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

#include <osg/Node>
#include <osg/Array>
#include <osg/Image>
#include <osg/Matrix>

#include <osg2vsg/Export.h>

namespace osg2vsg
{

    inline vsg::dmat4 convert(const osg::Matrixd& m)
    {
        return vsg::dmat4(m(0, 0), m(0, 1), m(0, 2), m(0, 3),
                          m(1, 0), m(1, 1), m(1, 2), m(1, 3),
                          m(2, 0), m(2, 1), m(2, 2), m(2, 3),
                          m(3, 0), m(3, 1), m(3, 2), m(3, 3));
    }

    inline vsg::mat4 convert(const osg::Matrixf& m)
    {
        return vsg::mat4(m(0, 0), m(0, 1), m(0, 2), m(0, 3),
                         m(1, 0), m(1, 1), m(1, 2), m(1, 3),
                         m(2, 0), m(2, 1), m(2, 2), m(2, 3),
                         m(3, 0), m(3, 1), m(3, 2), m(3, 3));
    }

    template<class T>
    vsg::ref_ptr<T> convert(const osg::Array& array)
    {
        auto new_array = T::create(array.getNumElements());
        std::memcpy(new_array->dataPointer(), array.getDataPointer(), array.getTotalDataSize());
        return new_array;
    }

    OSG2VSG_DECLSPEC extern vsg::ref_ptr<vsg::Data> convert(const osg::Array& array, vsg::ref_ptr<const vsg::Options> options = {});
    OSG2VSG_DECLSPEC extern vsg::ref_ptr<vsg::Data> convert(const osg::Image& image, vsg::ref_ptr<const vsg::Options> options = {});
    OSG2VSG_DECLSPEC extern vsg::ref_ptr<vsg::Node> convert(const osg::Node& node, vsg::ref_ptr<const vsg::Options> options = {});

} // namespace vsgXchange
