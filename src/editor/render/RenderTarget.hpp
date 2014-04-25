#ifndef RENDER_RENDERTARGET_HPP_
#define RENDER_RENDERTARGET_HPP_

#include <GL/glew.h>
#include <stack>

namespace Tungsten
{

namespace GL
{

class Texture;

enum RtAttachment {
    RT_ATTACHMENT0,
    RT_ATTACHMENT1,
    RT_ATTACHMENT2,
    RT_ATTACHMENT3,
    RT_ATTACHMENT4,
    RT_ATTACHMENT5,
    RT_ATTACHMENT6,
    RT_ATTACHMENT7
};

struct Viewport {
    int x, y, w, h;

    Viewport(int _x, int _y, int _w, int _h)
    : x(_x), y(_y), w(_w), h(_h)
    {}
};

class RenderTarget {
    static std::stack<Viewport> _viewports;
    static const int MaxAttachments = 8;

    static int _viewportX, _viewportY;
    static int _viewportW, _viewportH;

    int _nextTicket;
    int _attachmentTicket[MaxAttachments];
    const Texture *_attachments[MaxAttachments];

    int _attachmentCount;
    RtAttachment _selectedAttachments[MaxAttachments];

    GLuint _glName;

    bool attachmentSwapRequired(int num, const RtAttachment bufs[]);

public:
    RenderTarget();
    ~RenderTarget();

    void selectAttachments(int num);
    void selectAttachmentList(int count, ...);
    void selectAttachments(int num, const RtAttachment bufs[]);
    void setReadBuffer(RtAttachment buf);

    RtAttachment attachTextureAny(const Texture &tex);
    void attachTexture(const Texture &tex, int index, int level = 0);
    void attachDepthBuffer(const Texture &tex);
    void attachDepthStencilBuffer(const Texture &tex);
    void detachTexture(int index);
    void detachDepthBuffer();
    void detachDepthStencilBuffer();

    void bind();

    GLuint glName() const
    {
        return _glName;
    }

    static void unbind();
    static void resetViewport();
    static void setViewport(int x, int y, int w, int h);
    static void getViewport(int &x, int &y, int &w, int &h);
    static void pushViewport(int x, int y, int w, int h);
    static void popViewport();

    static int viewportX()
    {
        return _viewportX;
    }

    static int viewportY()
    {
        return _viewportY;
    }

    static int viewportW()
    {
        return _viewportW;
    }

    static int viewportH()
    {
        return _viewportH;
    }
};

}

}

#endif // RENDER_RENDERTARGET_HPP_
