#include "OpenGL.hpp"

#include "RenderTarget.hpp"
#include "Texture.hpp"
#include "Debug.hpp"

namespace Tungsten {

namespace GL {

std::stack<Viewport> RenderTarget::_viewports;
int RenderTarget::_viewportX = -1;
int RenderTarget::_viewportY = -1;
int RenderTarget::_viewportW = -1;
int RenderTarget::_viewportH = -1;

RenderTarget::RenderTarget()
{
    glf->glGenFramebuffers(1, &_glName);
}

RenderTarget::~RenderTarget()
{
    if (_glName)
        glf->glDeleteFramebuffers(1, &_glName);
}

RenderTarget::RenderTarget(RenderTarget &&o)
{
    _glName = o._glName;
    o._glName = 0;
}

RenderTarget &RenderTarget::operator=(RenderTarget &&o)
{
    _glName = o._glName;
    o._glName = 0;

    return *this;
}

void RenderTarget::selectAttachments(int num)
{
    if (num == 0) {
        GLenum target = GL_NONE;
        glf->glDrawBuffers(1, &target);
    } else {
        GLenum selected[RT_ATTACHMENT_COUNT];
        for (int i = 0; i < num; ++i)
            selected[i] = GL_COLOR_ATTACHMENT0 + i;

        glf->glDrawBuffers(num, selected);
    }
}

void RenderTarget::setReadBuffer(RtAttachment buf)
{
    glf->glReadBuffer(GL_COLOR_ATTACHMENT0 + static_cast<int>(buf));
}

void RenderTarget::attachTexture(const Texture &tex, int index, int level)
{
    switch(tex.type()) {
    case TEXTURE_BUFFER:
        FAIL("Cannot attach texture buffer to FBO\n");
        break;
    case TEXTURE_1D:
        glf->glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_1D, tex.glName(), level);
        break;
    case TEXTURE_2D:
        glf->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0  + index, GL_TEXTURE_2D, tex.glName(), level);
        break;
    case TEXTURE_CUBE:
    case TEXTURE_3D:
        glf->glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, tex.glName(), level);
        break;
    }
}

void RenderTarget::detachTexture(int index)
{
    glf->glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, 0, 0);
}

void RenderTarget::attachDepthBuffer(const Texture &tex)
{
    glf->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex.glName(), 0);
}

void RenderTarget::attachDepthStencilBuffer(const Texture &tex)
{
    glf->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex.glName(), 0);
}

void RenderTarget::detachDepthBuffer()
{
    glf->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

void RenderTarget::detachDepthStencilBuffer()
{
    glf->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

void RenderTarget::bind()
{
    glf->glBindFramebuffer(GL_FRAMEBUFFER, _glName);
}

void RenderTarget::unbind()
{
    glf->glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void RenderTarget::resetViewport()
{
    int viewport[4];
    glf->glGetIntegerv(GL_VIEWPORT, viewport);

    _viewportX = viewport[0];
    _viewportY = viewport[1];
    _viewportW = viewport[2];
    _viewportH = viewport[3];
}

void RenderTarget::setViewport(int x, int y, int w, int h)
{
    if (_viewportX != x || _viewportY != y || _viewportW != w || _viewportH != h) {
        glf->glViewport(x, y, w, h);

        _viewportX = x;
        _viewportY = y;
        _viewportW = w;
        _viewportH = h;
    }
}

void RenderTarget::getViewport(int &x, int &y, int &w, int &h)
{
    x = _viewportX;
    y = _viewportY;
    w = _viewportW;
    h = _viewportH;
}

void RenderTarget::pushViewport(int x, int y, int w, int h)
{
    _viewports.push(Viewport(_viewportX, _viewportY, _viewportW, _viewportH));
    setViewport(x, y, w, h);
}

void RenderTarget::popViewport()
{
    Viewport top = _viewports.top();
    _viewports.pop();
    setViewport(top.x, top.y, top.w, top.h);
}

}

}
