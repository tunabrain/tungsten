#ifndef COREPROFILECONTEXT_HPP_
#define COREPROFILECONTEXT_HPP_

#include <QtGlobal>
#include <QGLContext>

// QT4 does not support OpenGL 3.2 Core Profile on OSX.
// CoreProfileContext implements a compatibility wrapper.

#if defined(Q_OS_MAC)
#if QT_VERSION < 0x050000 && QT_VERSION >= 0x040800
void *chooseMacVisualCoreProfile(GDHandle handle, int depthBufferSize);
#endif
#endif

struct CoreProfileContext : public QGLContext {
    CoreProfileContext(const QGLFormat& format, QPaintDevice *device) : QGLContext(format, device) {}
    CoreProfileContext(const QGLFormat& format) : QGLContext(format) {}

#if defined(Q_OS_MAC)
#if QT_VERSION < 0x050000 && QT_VERSION >= 0x040800
    virtual void *chooseMacVisual(GDHandle handle)
    {
        return chooseMacVisualCoreProfile(handle, this->format().depthBufferSize());
    }
#endif
#endif
};

#endif // COREPROFILECONTEXT_HPP_
