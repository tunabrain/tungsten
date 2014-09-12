#include <GL/glew.h>
#include <cstdarg>

#include "RenderTarget.hpp"
#include "Texture.hpp"
#include "Debug.hpp"

namespace Tungsten {

namespace GL {

/* Future proof as hell */
static const GLenum targets[] = {
    GL_COLOR_ATTACHMENT0,  GL_COLOR_ATTACHMENT1,  GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3,  GL_COLOR_ATTACHMENT4,  GL_COLOR_ATTACHMENT5,
    GL_COLOR_ATTACHMENT6,  GL_COLOR_ATTACHMENT7,  GL_COLOR_ATTACHMENT8,
    GL_COLOR_ATTACHMENT9,  GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11,
    GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14,
    GL_COLOR_ATTACHMENT15,
};

static const RtAttachment simpleOrder[] = {
    RT_ATTACHMENT0, RT_ATTACHMENT1, RT_ATTACHMENT2, RT_ATTACHMENT3,
    RT_ATTACHMENT4, RT_ATTACHMENT5, RT_ATTACHMENT6, RT_ATTACHMENT7,
};

std::stack<Viewport> RenderTarget::_viewports;
int RenderTarget::_viewportX = -1;
int RenderTarget::_viewportY = -1;
int RenderTarget::_viewportW = -1;
int RenderTarget::_viewportH = -1;

RenderTarget::RenderTarget()
{
    glGenFramebuffers(1, &_glName);

    _attachmentCount = 1;
    for (int i = 0; i < MaxAttachments; i++) {
        _attachments     [i] = 0;
        _attachmentTicket[i] = 0;

        _selectedAttachments[i] = RT_ATTACHMENT0;
    }
    _nextTicket = 1;
}

RenderTarget::~RenderTarget()
{
    glDeleteFramebuffers(1, &_glName);
}

bool RenderTarget::attachmentSwapRequired(int num, const RtAttachment bufs[])
{
    return true;
    if (num != _attachmentCount)
        return true;

    for (int i = 0; i < num; i++)
        if (_selectedAttachments[i] != bufs[i])
            return true;

    return false;
}

void RenderTarget::selectAttachments(int num)
{
    selectAttachments(num, simpleOrder);
}

void RenderTarget::selectAttachmentList(int num, ...)
{
    if (!num)
        selectAttachments(0, 0);

    va_list args;
    va_start(args, num);

    RtAttachment selected[MaxAttachments];
    for (int i = 0; i < num; i++)
        selected[i] = (RtAttachment)va_arg(args, int);

    va_end(args);

    selectAttachments(num, selected);
}

void RenderTarget::selectAttachments(int num, const RtAttachment *bufs)
{
    if (attachmentSwapRequired(num, bufs)) {
        _attachmentCount = num;

        if (num == 0) {
            GLenum target = GL_NONE;
            glDrawBuffers(1, &target);
        } else {
            GLenum selected[MaxAttachments];

            for (int i = num - 1; i >= 0; i--) {
                selected[i] = targets[int(bufs[i])];
                _selectedAttachments[i] = bufs[i];
            }

            glDrawBuffers(num, selected);
        }
    }
}

void RenderTarget::setReadBuffer(RtAttachment buf)
{
    glReadBuffer(targets[buf]);
}

RtAttachment RenderTarget::attachTextureAny(const Texture &tex)
{
    for (int i = 0; i < MaxAttachments; i++) {
        if (_attachments[i] == &tex) {
            _attachmentTicket[i] = _nextTicket++;
            return simpleOrder[i];
        }
    }

    for (int i = 0; i < MaxAttachments; i++)
        if (_attachments[i] && (_attachments[i]->width() != tex.width() || _attachments[i]->height() != tex.height()))
            detachTexture(i);

    int leastTicket = _attachmentTicket[0], leastAttachment = 0;
    for (int i = 1; i < MaxAttachments; i++) {
        if (_attachmentTicket[i] < leastTicket) {
            leastTicket     = _attachmentTicket[i];
            leastAttachment = i;
        }
    }

    attachTexture(tex, leastAttachment);
    return simpleOrder[leastAttachment];
}

void RenderTarget::attachTexture(const Texture &tex, int index, int level)
{
    _attachmentTicket[index] = _nextTicket++;
    _attachments     [index] = &tex;

    switch(tex.type()) {
    case TEXTURE_BUFFER:
        FAIL("Cannot attach texture buffer to FBO\n");
        break;
    case TEXTURE_1D:
        glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_1D, tex.glName(), level);
        break;
    case TEXTURE_2D:
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, tex.glName(), level);
        break;
    case TEXTURE_CUBE:
    case TEXTURE_3D:
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, tex.glName(), level);
        break;
    }
}

void RenderTarget::detachTexture(int index)
{
    if (_attachments[index]) {
        _attachments[index] = 0;
        _attachmentTicket[index] = 0;
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, 0, 0);
    }
}

void RenderTarget::attachDepthBuffer(const Texture &tex)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex.glName(), 0);
}

void RenderTarget::attachDepthStencilBuffer(const Texture &tex)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex.glName(), 0);
}

void RenderTarget::detachDepthBuffer()
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

void RenderTarget::detachDepthStencilBuffer()
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

void RenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, _glName);
}

void RenderTarget::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void RenderTarget::resetViewport()
{
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    _viewportX = viewport[0];
    _viewportY = viewport[1];
    _viewportW = viewport[2];
    _viewportH = viewport[3];
}

void RenderTarget::setViewport(int x, int y, int w, int h)
{
    if (_viewportX != x || _viewportY != y || _viewportW != w || _viewportH != h) {
        glViewport(x, y, w, h);

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
