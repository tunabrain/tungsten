#include <QGLContext>

#import <AppKit/AppKit.h>

#if QT_VERSION < 0x050000
void *chooseMacVisualCoreProfile(GDHandle handle, int depthBufferSize)
{
    static const int NumAttribs = 40;
    NSOpenGLPixelFormatAttribute attribs[NumAttribs];
    int cnt = 0;

    attribs[cnt++] = NSOpenGLPFAOpenGLProfile;
    attribs[cnt++] = NSOpenGLProfileVersion3_2Core;

    attribs[cnt++] = NSOpenGLPFADoubleBuffer;

    attribs[cnt++] = NSOpenGLPFADepthSize;
    attribs[cnt++] = (NSOpenGLPixelFormatAttribute) (depthBufferSize == -1) ? 32 : depthBufferSize;

    attribs[cnt] = 0;
    Q_ASSERT(cnt < NumAttribs);

    return [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
}
#endif
