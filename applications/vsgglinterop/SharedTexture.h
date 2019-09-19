/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#pragma once

#include <osg/StateAttribute>
#include <osg/Texture>

#if WIN32
#include <Windows.h>
#endif

namespace osg
{
    class /*OSG_EXPORT*/ SharedTexture : public Texture
    {
    public:

#if WIN32
        typedef HANDLE HandleType;
#else
        typedef int HandleType;
#endif

        SharedTexture();
        SharedTexture(HandleType handle, GLuint64 byteSize, int width, int height, GLenum format, GLuint tiling, bool isDedicated);

        /** Copy constructor using CopyOp to manage deep vs shallow copy. */
        SharedTexture(const SharedTexture& text, const CopyOp& copyop = CopyOp::SHALLOW_COPY) :
            Texture(text, copyop) {}

        META_StateAttribute(osg, SharedTexture, TEXTURE);

        /** Return -1 if *this < *rhs, 0 if *this==*rhs, 1 if *this>*rhs. */
        virtual int compare(const StateAttribute& rhs) const { return (this<&rhs) ? -1 : ((this==&rhs) ? 0 : 1 ); }

        virtual GLenum getTextureTarget() const { return GL_TEXTURE_2D; }


        /** Sets the texture image. */
        void setImage(Image* image) { _image = image; }

        template<class T> void setImage(const ref_ptr<T>& image) { setImage(image.get()); }

        /** Gets the texture image. */
        Image* getImage() { return _image.get(); }

        /** Gets the const texture image. */
        inline const Image* getImage() const { return _image.get(); }

        /** Sets the texture image, ignoring face. */
        virtual void setImage(unsigned int, Image* image) { setImage(image); }

        template<class T> void setImage(unsigned int, const ref_ptr<T>& image) { setImage(image.get()); }

        /** Gets the texture image, ignoring face. */
        virtual Image* getImage(unsigned int) { return _image.get(); }

        /** Gets the const texture image, ignoring face. */
        virtual const Image* getImage(unsigned int) const { return _image.get(); }

        /** Gets the number of images that can be assigned to the Texture. */
        virtual unsigned int getNumImages() const { return 1; }


        virtual void apply(State& state) const;

        static std::vector<GLint> getSupportedTilingTypesForFormat(const osg::State& state, GLenum format = GL_RGBA8);

    protected:
        virtual ~SharedTexture();

        virtual void computeInternalFormat() const { computeInternalFormatType(); }
        void allocateMipmap(State& /*state*/) const {}

        GLuint64 _byteSize;
        GLsizei _textureWidth;
        GLsizei _textureHeight;

        HandleType _handle;
        GLuint _tiling;
        //GLuint _memory;
        bool _isDedicated;

        osg::ref_ptr<osg::Image> _image;
    };
}

