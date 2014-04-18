TEMPLATE = app
TARGET = editor

QT = core gui opengl

LIBS += -lcore
include(../../shared.pri)
PRE_TARGETDEPS += $$quote($$CORELIBFILE)

SOURCES += \
    main.cpp \
    ShapeInput.cpp \
    MainWindow.cpp \
    RenderWindow.cpp \
    ShapePainter.cpp \
    PreviewWindow.cpp \
    TransformGizmo.cpp \
    render/Shader.cpp \
    render/Texture.cpp \
    render/MatrixStack.cpp \
    render/BufferObject.cpp \
    render/RenderTarget.cpp \
    render/ShaderObject.cpp \
    render/VertexBuffer.cpp
    
HEADERS += \
    MainWindow.hpp \
    ShapeInput.hpp \
    RenderWindow.hpp \
    ShapePainter.hpp \
    PreviewWindow.hpp \
    CameraControls.hpp \
    TransformGizmo.hpp \
    render/Shader.hpp \
    render/Texture.hpp \
    render/MatrixStack.hpp \
    render/BufferObject.hpp \
    render/RenderTarget.hpp \
    render/ShaderObject.hpp \
    render/VertexBuffer.hpp
