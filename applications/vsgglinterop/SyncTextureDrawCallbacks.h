/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#pragma once

#include <osg/Camera>
#include <osg/RenderInfo>
#include <osg/Texture2D>

#include "SharedSemaphore.h"
#include "GLMemoryExtensions.h"

namespace osg
{
    class SemaphoreDrawCallback : public osg::Camera::DrawCallback
    {
    public:

        SemaphoreDrawCallback(SharedTexture* tex, GLenum layout, ref_ptr<GLSemaphore> semaphore, bool wait) :
            osg::Camera::DrawCallback(),
            _texture(tex),
            _layout(layout),
            _semaphore(semaphore),
            _wait(wait)
        {
        }

        virtual void operator () (osg::RenderInfo& renderInfo) const
        {
            GLMemoryExtensions* extensions = GLMemoryExtensions::Get(renderInfo.getContextID(), true);

            std::vector<GLuint> semaphoreBuffers;
            std::vector<GLuint> semaphoreTextures;
            semaphoreTextures.push_back(_texture->getTextureObject(renderInfo.getContextID())->id());

            if(_wait)
            {
                // wait on the semaphone
                _semaphore->wait(*renderInfo.getState(), semaphoreBuffers, semaphoreTextures, {_layout});
            }
            else
            {
                // signal complete semaphone
                _semaphore->signal(*renderInfo.getState(), semaphoreBuffers, semaphoreTextures, {_layout});
            }

        }

    protected:
        virtual ~SemaphoreDrawCallback() {}

        osg::ref_ptr<SharedTexture> _texture;
        GLenum _layout;

        ref_ptr<GLSemaphore> _semaphore;
        bool _wait;
    };

    class SyncTextureDrawCallback : public osg::Camera::DrawCallback
    {
    public:
        using SyncTextureMappings = std::map<Texture*, Texture*>;

        SyncTextureDrawCallback(SyncTextureMappings mappings, bool syncTo, std::vector<GLenum> srcLayouts, std::vector<GLenum> dstLayouts, ref_ptr<GLSemaphore> waitSemaphore, ref_ptr<GLSemaphore> completeSemaphore) :
            osg::Camera::DrawCallback(),
            _mappings(mappings),
            _syncTo(syncTo),
            _srcLayouts(srcLayouts),
            _dstLayouts(dstLayouts),
            _waitSemaphore(waitSemaphore),
            _completeSemaphore(completeSemaphore)
        {
        }

        virtual void operator () (osg::RenderInfo& renderInfo) const
        {
            GLMemoryExtensions* extensions = GLMemoryExtensions::Get(renderInfo.getContextID(), true);

            std::vector<GLuint> semaphoreBuffers;
            std::vector<GLuint> semaphoreTextures;
            for (auto const& texpair : _mappings)
            {
                semaphoreTextures.push_back(texpair.second->getTextureObject(renderInfo.getContextID())->id());
            }

            // wait on the semaphone
            _waitSemaphore->wait(*renderInfo.getState(), semaphoreBuffers, semaphoreTextures, _srcLayouts);

            // copy the textures into the shared textures
            for (auto const& texpair : _mappings)
            {
                GLenum srcId = _syncTo ? texpair.first->getTextureObject(renderInfo.getContextID())->id() : texpair.second->getTextureObject(renderInfo.getContextID())->id();
                GLenum dstId = _syncTo ? texpair.second->getTextureObject(renderInfo.getContextID())->id() : texpair.first->getTextureObject(renderInfo.getContextID())->id();
                extensions->glCopyImageSubData(srcId, GL_TEXTURE_2D, 0, 0, 0, 0, dstId, GL_TEXTURE_2D, 0, 0, 0, 0, texpair.first->getTextureWidth(), texpair.first->getTextureHeight(), 1);
            }

            // signal complete semaphone
            _completeSemaphore->signal(*renderInfo.getState(), semaphoreBuffers, semaphoreTextures, _dstLayouts);
        }

    protected:
        virtual ~SyncTextureDrawCallback() {}

        SyncTextureMappings _mappings;
        bool _syncTo;

        std::vector<GLenum> _srcLayouts;
        std::vector<GLenum> _dstLayouts;

        ref_ptr<GLSemaphore> _waitSemaphore;
        ref_ptr<GLSemaphore> _completeSemaphore;
    };
}

