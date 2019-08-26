/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#ifndef OSG_SHAREDTEXTURE
#define OSG_SHAREDTEXTURE 1

#include <osg/StateAttribute>
#include <osg/Texture>

#if defined(WIN32)
#include <Windows.h>
typedef HANDLE HandleType;
#else
typedef int HandleType;
#endif

#ifndef GL_EXT_memory_object
#define GL_TEXTURE_TILING_EXT             0x9580
#define GL_DEDICATED_MEMORY_OBJECT_EXT    0x9581
#define GL_PROTECTED_MEMORY_OBJECT_EXT    0x959B
#define GL_NUM_TILING_TYPES_EXT           0x9582
#define GL_TILING_TYPES_EXT               0x9583
#define GL_OPTIMAL_TILING_EXT             0x9584
#define GL_LINEAR_TILING_EXT              0x9585
#endif

#ifndef GL_EXT_memory_object_win32
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT   0x9587
#endif

namespace osg
{

    class /*OSG_EXPORT*/ SharedTexture : public Texture
    {
    public:
        SharedTexture();
        SharedTexture(HandleType handle, GLuint64 byteSize, int width, int height, GLenum format, GLuint tiling, bool isDedicated);

        /** Copy constructor using CopyOp to manage deep vs shallow copy. */
        SharedTexture(const SharedTexture& text, const CopyOp& copyop = CopyOp::SHALLOW_COPY) {}

        META_StateAttribute(osg, SharedTexture, TEXTURE);

        /** Return -1 if *this < *rhs, 0 if *this==*rhs, 1 if *this>*rhs. */
        virtual int compare(const StateAttribute& rhs) const { return -1; }

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

    protected:
        virtual ~SharedTexture();

        virtual void computeInternalFormat() const { computeInternalFormatType(); }

        void allocateMipmap(State& state) const {}

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

#endif

