#ifndef PREVIEWWINDOW_HPP_
#define PREVIEWWINDOW_HPP_

#include "CameraControls.hpp"
#include "TransformGizmo.hpp"

#include "opengl/BufferObject.hpp"
#include "opengl/VertexBuffer.hpp"
#include "opengl/RenderTarget.hpp"
#include "opengl/Texture.hpp"
#include "opengl/Shader.hpp"

#include "math/Vec.hpp"

#include <unordered_map>
#include <unordered_set>
#include <QGLWidget>
#include <memory>

class QStatusBar;

namespace Tungsten {

class MainWindow;
class TriangleMesh;
class Primitive;
class Scene;

class GlMesh
{
    struct VboVertex
    {
        Vec3f pos;
        Vec3f normal;
        Vec2f texCoord;
    };
    struct VboTriangle
    {
        uint32 v0;
        uint32 v1;
        uint32 v2;
    };

    GL::VertexBuffer _vertexBuffer;
    GL::BufferObject _indexBuffer;

public:
    GlMesh(const TriangleMesh &src);

    void draw(GL::Shader &shader);
};

enum MouseConsumers
{
    CameraConsumer    = (1 << 0),
    GizmoConsumer     = (1 << 1),
    SelectionConsumer = (1 << 2),
};

class PreviewWindow : public QGLWidget
{
    Q_OBJECT

    const float Fov  = 60.0f;
    const float Near = 0.01f;
    const float Far  = 100.0f;

    struct SelectionState
    {
        int startX, startY;
        int endX, endY;
        bool selecting;
        bool shiftDown;
        bool finished;

        SelectionState()
        : selecting(false), shiftDown(false), finished(false)
        {
        }
    };

    MainWindow &_parent;

    CameraControls _controls;
    Vec3f _initialPos, _initialTarget, _initialUp;
    TransformGizmo _gizmo;

    Scene *_scene;

    std::unique_ptr<GL::RenderTarget> _fbo;
    std::unique_ptr<GL::Texture> _screenBuffer, _depthBuffer;

    std::unordered_map<Primitive *, std::shared_ptr<GlMesh>> _meshes;
    std::unordered_set<Primitive *> _dirtyPrimitives;
    std::unique_ptr<GL::Shader> _shader, _wireframeShader, _solidShader;

    SelectionState _selectionState;
    std::unordered_set<Primitive *> &_selection;

    std::array<MouseConsumers, 3> _mousePriorities;

    bool _rebuildMeshes;

    Mat4f projection() const;

    void rebuildMeshMap();
    bool updateViewTransform(QMouseEvent *event);
    void updateFixedTransform();

    template<typename Predicate>
    void renderMeshes(GL::Shader &shader, Predicate predicate);

    void pickPrimitive();
    bool handleSelection(QMouseEvent *event);

    bool handleMouse(QMouseEvent *event);

    void drawBackgroundGradient();
    void drawGrid();

private slots:
    void toggleSelectAll();
    void grabGizmo();
    void transformFinished(Mat4f delta);
    void recomputeCentroids();
    void computeHardNormals();
    void computeSmoothNormals();
    void freezeTransforms();
    void duplicateSelection();
    void addModel();
    void deleteSelection();
    void togglePreview();
    void resetCamera();
    void showContextMenu();

protected:
    virtual void initializeGL() override final;
    virtual void paintGL() override final;
    virtual void resizeGL(int width, int height) override final;

    virtual void mouseMoveEvent(QMouseEvent *eventMove);
    virtual void mousePressEvent(QMouseEvent *eventPress);
    virtual void mouseReleaseEvent(QMouseEvent *releaseEvent);

    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

public slots:
    void sceneChanged();
    void changeSelection();

signals:
    void selectionChanged();
    void primitiveListChanged();

public:
    PreviewWindow(QWidget *proxyParent, MainWindow *parent);

    void addStatusWidgets(QStatusBar *statusBar);
    void saveSceneData();
};

}

#endif /* PREVIEWWINDOW_HPP_ */
