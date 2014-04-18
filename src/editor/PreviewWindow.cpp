#include <GL/glew.h>

#include <QtCore/QtCore>
#include <QtGui>

#include "PreviewWindow.hpp"
#include "ShapePainter.hpp"
#include "MainWindow.hpp"
#include "Camera.hpp"

#include "primitives/Mesh.hpp"

#include "render/MatrixStack.hpp"
#include "render/GLDebug.hpp"

#include "lights/ConeLight.hpp"
#include "lights/DirectionalLight.hpp"
#include "lights/EnvironmentLight.hpp"
#include "lights/PointLight.hpp"
#include "lights/QuadLight.hpp"
#include "lights/SphereLight.hpp"

#include "io/ObjLoader.hpp"

namespace Tungsten
{

static void GLAPIENTRY errorCallback(GLenum source, GLenum type, GLuint /*id*/, GLenum severity,
        GLsizei /*length*/, const char *message, void */*userParam*/) {

    const char *sourceN = 0, *typeN = 0, *severityN = 0;

    switch(source) {
    case GL_DEBUG_SOURCE_API_ARB:
        sourceN = "API"; break;
    case GL_DEBUG_SOURCE_APPLICATION_ARB:
        sourceN = "Application"; break;
    case GL_DEBUG_SOURCE_OTHER_ARB:
        sourceN = "Other"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
        sourceN = "Shader compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
        sourceN = "Third party"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
        sourceN = "Window system"; break;
    }

    switch(type) {
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
        typeN = "Deprecated behavior"; break;
    case GL_DEBUG_TYPE_ERROR_ARB:
        typeN = "Error"; break;
    case GL_DEBUG_TYPE_OTHER_ARB:
        typeN = "Other"; break;
    case GL_DEBUG_TYPE_PERFORMANCE_ARB:
        typeN = "Performance"; break;
    case GL_DEBUG_TYPE_PORTABILITY_ARB:
        typeN = "Portability"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
        typeN = "Undefined behavior"; break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH_ARB:
        severityN = "High"; break;
    case GL_DEBUG_SEVERITY_MEDIUM_ARB:
        severityN = "Medium"; break;
    case GL_DEBUG_SEVERITY_LOW_ARB:
        severityN = "Low"; break;
    }

    LOG("GL", WARN, "Severity: %s. Source: %s. Type: %s. Message: %s\n", severityN, sourceN, typeN, message);
}

GlMesh::GlMesh(const TriangleMesh &src)
: _vertexBuffer(src.verts().size()),
  _indexBuffer(ELEMENT_ARRAY_BUFFER, src.tris().size()*3*sizeof(uint32))
{
    _vertexBuffer.setStandardAttributes(VBO_ATT_POSITION | VBO_ATT_NORMAL | VBO_ATT_TEXCOORD0);
    _vertexBuffer.initBuffer();

    _vertexBuffer.bind();
    VboVertex *verts = _vertexBuffer.map<VboVertex>();
    for (const Vertex &v : src.verts())
        *(verts++) = VboVertex{v.pos(), v.normal(), v.uv()};
    _vertexBuffer.unmap();

    _indexBuffer.bind();
    VboTriangle *tris = _indexBuffer.map<VboTriangle>();
    for (const TriangleI &t : src.tris())
        *(tris++) = VboTriangle{t.v0, t.v1, t.v2};
    _indexBuffer.unmap();
}

void GlMesh::draw(Shader &shader)
{
    _vertexBuffer.drawIndexed(_indexBuffer, shader, GL_TRIANGLES);
}

PreviewWindow::PreviewWindow(QWidget *proxyParent, MainWindow *parent, const QGLFormat &format)
: QGLWidget(format, proxyParent),
  _parent(*parent),
  _scene(parent->scene()),
  _mousePriorities(DefaultPriorities),
  _rebuildMeshes(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    new QShortcut(QKeySequence("A"), this, SLOT(toggleSelectAll()));
    new QShortcut(QKeySequence("G"), this, SLOT(grabGizmo()));
    new QShortcut(QKeySequence("Ctrl+C"), this, SLOT(recomputeCentroids()));
    new QShortcut(QKeySequence("Ctrl+R"), this, SLOT(computeSmoothNormals()));
    new QShortcut(QKeySequence("Ctrl+Shift+R"), this, SLOT(computeHardNormals()));
    new QShortcut(QKeySequence("Ctrl+F"), this, SLOT(freezeTransforms()));
    new QShortcut(QKeySequence("Ctrl+D"), this, SLOT(duplicateSelection()));
    new QShortcut(QKeySequence("Ctrl+A"), this, SLOT(addModel()));
    new QShortcut(QKeySequence("Delete"), this, SLOT(deleteSelection()));
    new QShortcut(QKeySequence("Tab"), this, SLOT(togglePreview()));

    QShortcut *tShortcut = new QShortcut(QKeySequence("W"), this);
    QShortcut *rShortcut = new QShortcut(QKeySequence("E"), this);
    QShortcut *sShortcut = new QShortcut(QKeySequence("R"), this);
    QShortcut *xShortcut = new QShortcut(QKeySequence("X"), this);
    QShortcut *yShortcut = new QShortcut(QKeySequence("Y"), this);
    QShortcut *zShortcut = new QShortcut(QKeySequence("Z"), this);
    QShortcut *lShortcut = new QShortcut(QKeySequence("Q"), this);

    QSignalMapper *gizmoMapper = new QSignalMapper();
    connect(tShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    connect(rShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    connect(sShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    gizmoMapper->setMapping(tShortcut, 0);
    gizmoMapper->setMapping(rShortcut, 1);
    gizmoMapper->setMapping(sShortcut, 2);
    connect(gizmoMapper, SIGNAL(mapped(int)), &_gizmo, SLOT(setMode(int)));

    gizmoMapper = new QSignalMapper();
    connect(xShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    connect(yShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    connect(zShortcut, SIGNAL(activated()), gizmoMapper, SLOT(map()));
    gizmoMapper->setMapping(xShortcut, 0);
    gizmoMapper->setMapping(yShortcut, 1);
    gizmoMapper->setMapping(zShortcut, 2);
    connect(gizmoMapper, SIGNAL(mapped(int)), &_gizmo, SLOT(fixAxis(int)));
    connect(lShortcut, SIGNAL(activated()), &_gizmo, SLOT(toggleTranslateLocal()));
    connect(&_gizmo, SIGNAL(redraw()), this, SLOT(updateGL()));
    connect(&_gizmo, SIGNAL(transformFinished(Mat4f)), this, SLOT(transformFinished(Mat4f)));
}

void PreviewWindow::addStatusWidgets(QStatusBar *statusBar)
{
    statusBar->addPermanentWidget(new QLabel(), 1);
}

void PreviewWindow::rebuildMeshMap()
{
    std::unordered_map<Entity *, std::shared_ptr<GlMesh>> tmpMap = std::move(_meshes);
    _meshes.clear();

    if (!_scene)
        return;

    for (Entity *e : _scene->entities()) {
        auto iter = tmpMap.find(e);

        if (iter == tmpMap.end())
            _meshes.insert(std::make_pair(e, std::make_shared<GlMesh>(e->asTriangleMesh())));
        else
            _meshes.insert(std::make_pair(e, iter->second));
    }
}

bool PreviewWindow::updateViewTransform(QMouseEvent *event)
{
    if (!_scene)
        return false;

    bool result = _controls.update(event, width(), height(), _scene->camera()->fov());
    _scene->camera()->setTransform(_controls.globalPos(), _controls.lookAt(), _controls.up());
    _gizmo.setView(_scene->camera()->transform());
    return result;
}

void PreviewWindow::updateFixedTransform()
{
    if (_selection.size() == 1) {
        _gizmo.setFixedTransform((*_selection.begin())->transform());
    } else if (!_selection.empty()) {
        Vec3f center(0.0f);
        for (Entity *e : _selection)
            center += e->transform()*Vec3f(0.0f);
        center /= _selection.size();

        _gizmo.setFixedTransform(Mat4f::translate(center));
    }
}

template<typename Predicate>
void PreviewWindow::renderMeshes(Shader &shader, Predicate predicate)
{
    const std::vector<Entity *> &entities = _scene->entities();
    for (size_t i = 0; i < entities.size(); ++i) {
        if (!predicate(i))
            continue;
        MatrixStack::set(MODEL_STACK, entities[i]->transform());
        if (_selection.count(entities[i]) && _gizmo.transforming())
            MatrixStack::mulL(MODEL_STACK, _gizmo.deltaTransform());

        MatrixStack::setShaderMatrices(shader, MODELVIEWPROJECTION_FLAG | MODEL_FLAG | VIEW_FLAG);
        shader.uniformI("Smooth", entities[i]->asTriangleMesh().smoothed());
        shader.uniformI("NoShading", dynamic_cast<Light *>(entities[i]) != nullptr);
        _meshes[entities[i]]->draw(shader);
    }
}

void PreviewWindow::pickPrimitive()
{
    const std::vector<Entity *> &prims = _scene->entities();

    _fbo->bind();
    _fbo->attachDepthBuffer(*_depthBuffer);
    _fbo->attachTexture(*_screenBuffer, 0);
    _fbo->selectAttachments(1);
    _fbo->setReadBuffer(RT_ATTACHMENT0);

    glViewport(0, 0, width(), height());

    glDisable(GL_MULTISAMPLE);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _solidShader->bind();
    renderMeshes(*_solidShader, [this](uint32 i) {
        _solidShader->uniformF(
            "Color",
            ((i      ) & 0xFF)/255.0f,
            ((i >>  8) & 0xFF)/255.0f,
            ((i >> 16) & 0xFF)/255.0f,
            ((i >> 24) & 0xFF)/255.0f
        );
        return true;
    });

    int startX = clamp(min(_selectionState.endX, _selectionState.startX), 0, width() - 1);
    int startY = clamp(min(_selectionState.endY, _selectionState.startY), 0, height() - 1);
    int w = min(std::abs(_selectionState.endX - _selectionState.startX), width() - startX);
    int h = min(std::abs(_selectionState.endY - _selectionState.startY), height() - startY);

    std::unordered_set<uint32> uniquePixels;
    std::vector<uint32> buffer(max(w, 1)*max(h, 1));
    glReadPixels(startX, height() - startY - h - 1, max(w, 1), max(h, 1), GL_RGBA, GL_UNSIGNED_BYTE, &buffer[0]);

    _fbo->unbind();
    glEnable(GL_MULTISAMPLE);

    for (uint32 pixel : buffer) {
        if (pixel != 0xFFFFFFFF) {
            ASSERT(pixel < prims.size(), "Invalid prim id: %d", pixel);
            uniquePixels.insert(pixel);
        }
    }

    if (_selectionState.shiftDown) {
        if (w == 0 && h == 0 && !uniquePixels.empty()) {
            Entity *prim = prims[*uniquePixels.begin()];
            if (_selection.count(prim))
                _selection.erase(prim);
            else
                _selection.insert(prim);
        } else {
            for (uint32 id : uniquePixels)
                _selection.insert(prims[id]);
        }
    } else {
        _selection.clear();
        for (uint32 id : uniquePixels)
            _selection.insert(prims[id]);
    }

    _selectionState = SelectionState();
    updateFixedTransform();
}

bool PreviewWindow::handleSelection(QMouseEvent *event)
{
    _selectionState.shiftDown = event->modifiers() & Qt::ShiftModifier;
    if (_selectionState.selecting) {
        _selectionState.endX = event->pos().x();
        _selectionState.endY = event->pos().y();
        if (!(event->buttons() & Qt::LeftButton)) {
            _selectionState.finished = true;
        }
        return true;
    } else if (event->buttons() & Qt::LeftButton) {
        _selectionState.selecting = true;
        _selectionState.endX = _selectionState.startX = event->pos().x();
        _selectionState.endY = _selectionState.startY = event->pos().y();
        return true;
    }
    return false;
}

bool PreviewWindow::handleMouse(QMouseEvent *event)
{
    bool caughtMouse = false;
    for (size_t i = 0; i < _mousePriorities.size(); ++i)
    {
        switch (_mousePriorities[i]) {
        case CameraConsumer:
            caughtMouse = updateViewTransform(event);
            break;
        case GizmoConsumer:
            caughtMouse = _gizmo.update(event);
            break;
        case SelectionConsumer:
            caughtMouse = handleSelection(event);
            break;
        }
        if (caughtMouse) {
            std::swap(_mousePriorities[i], _mousePriorities[0]);
            break;
        }
    }
    updateGL();

    if (!caughtMouse) {
        _mousePriorities = DefaultPriorities;
    }

    return caughtMouse;
}

void PreviewWindow::toggleSelectAll()
{
    if (!_scene)
        return;

    if (_selection.size()) {
        _selection.clear();
    } else {
        for (Entity *e : _scene->entities())
            _selection.insert(e);
    }

    updateFixedTransform();
    updateGL();
}

void PreviewWindow::grabGizmo()
{
    if (!_selection.empty()) {
        QPoint p = mapFromGlobal(QCursor::pos());
        _gizmo.beginTransform(p.x(), p.y());
    }
}

void PreviewWindow::transformFinished(Mat4f delta)
{
    for (Entity *e : _selection)
        e->setTransform(delta*e->transform());
    updateFixedTransform();
    updateGL();
}

void PreviewWindow::recomputeCentroids()
{
    for (Entity *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            Vec3f centroid(0.0f);
            for (const Vertex &v : m->verts())
                centroid += v.pos();
            centroid /= m->verts().size();
            for (Vertex &v : m->verts())
                v.pos() -= centroid;
            m->setTransform(m->transform()*Mat4f::translate(centroid));
            m->markDirty();

            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    updateFixedTransform();
    updateGL();
}

void PreviewWindow::computeHardNormals()
{
    for (Entity *e : _selection)
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e))
            m->setSmoothed(false);
    updateGL();
}

void PreviewWindow::computeSmoothNormals()
{
    for (Entity *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            m->calcSmoothVertexNormals();
            m->setSmoothed(true);
            m->markDirty();
            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    updateGL();
}

void PreviewWindow::freezeTransforms()
{
    for (Entity *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            Mat4f tform = m->transform().stripTranslation();
            for (Vertex &v : m->verts())
                v.pos() = tform*v.pos();
            m->markDirty();
            m->setTransform(m->transform().extractTranslation());

            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    updateFixedTransform();
    updateGL();
}

void PreviewWindow::duplicateSelection()
{
    if (!_scene)
        return;

    size_t  primTail = _scene->primitives().size();
    size_t lightTail = _scene->lights().size();

    for (Entity *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e))
            _scene->addMesh(std::make_shared<TriangleMesh>(*m));
        else if (Light *l = dynamic_cast<Light *>(e))
            _scene->addLight(l->clone());
    }

    _selection.clear();
    for (size_t i = primTail; i < _scene->primitives().size(); ++i)
        _selection.insert(_scene->primitives()[i].get());
    for (size_t i = lightTail; i < _scene->lights().size(); ++i)
        _selection.insert(_scene->lights()[i].get());

    rebuildMeshMap();
    updateFixedTransform();
    updateGL();
}

void PreviewWindow::deleteSelection()
{
    if (!_scene)
        return;
    _scene->deleteEntities(_selection);
    _selection.clear();
    _gizmo.abortTransform();
    rebuildMeshMap();
    updateGL();
}

void PreviewWindow::addModel()
{
    if (!_scene)
        return;

    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Open file...",
        QString::fromStdString(FileUtils::getCurrentDir()),
        "Mesh files (*.obj;*.json)"
    );

    if (!file.isEmpty()) {
        std::string p = file.toStdString();
        std::string ext = FileUtils::extractExt(p);

        Scene *scene = nullptr;
        if (ext == "obj")
            scene = ObjLoader::load(p.c_str());
        else if (ext == "json")
            scene = Scene::load(p);
        if (!scene)
            return;

        size_t  primTail = _scene->primitives().size();
        size_t lightTail = _scene->lights().size();

        Mat4f tform = Mat4f::translate(_scene->camera()->lookAt() - scene->camera()->lookAt());

        _scene->merge(std::move(*scene));
        delete scene;


        _selection.clear();
        for (size_t i = primTail; i < _scene->primitives().size(); ++i) {
            _scene->primitives()[i]->setTransform(tform*_scene->primitives()[i]->transform());
            _selection.insert(_scene->primitives()[i].get());
        }
        for (size_t i = lightTail; i < _scene->lights().size(); ++i) {
            _scene->lights()[i]->setTransform(tform*_scene->lights()[i]->transform());
            _selection.insert(_scene->lights()[i].get());
        }

        rebuildMeshMap();
        updateGL();
    }
}

void PreviewWindow::togglePreview()
{
    if (!_gizmo.transforming())
        _parent.togglePreview();
}

void PreviewWindow::initializeGL()
{
    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();

    glDebugMessageCallbackARB((GLDEBUGPROCARB)errorCallback, NULL);
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    _fbo.reset(new RenderTarget());

    _shader.reset(
        new Shader("src/editor/shaders/", "Preamble.txt", "MeshPreview.vert", "MeshPreview.geom", "MeshPreview.frag", 1));
    _solidShader.reset(
        new Shader("src/editor/shaders/", "Preamble.txt", "SolidMesh.vert", nullptr, "SolidMesh.frag", 1));
    _wireframeShader.reset(
        new Shader("src/editor/shaders/", "Preamble.txt", "Wireframe.vert", "Wireframe.geom", "Wireframe.frag", 1));
}

void PreviewWindow::paintGL()
{
    if (_rebuildMeshes) {
        _rebuildMeshes = false;
        rebuildMeshMap();
    }

    glViewport(0, 0, width(), height());
    RenderTarget::resetViewport();

    if (!_screenBuffer && width() > 0 && height() > 0) {
        _screenBuffer.reset(new Texture(TEXTURE_2D, width(), height()));
        _screenBuffer->setFormat(TEXEL_FLOAT, 4, 1);
        _screenBuffer->init();
        _depthBuffer.reset(new Texture(TEXTURE_2D, width(), height()));
        _depthBuffer->setFormat(TEXEL_DEPTH, 1, 3);
        _depthBuffer->init();
    }

    if (_selectionState.finished && _scene)
        pickPrimitive();

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!_scene)
        return;

    std::shared_ptr<Camera> cam = _scene->camera();

    MatrixStack::set(VIEW_STACK, cam->transform());
    MatrixStack::set(PROJECTION_STACK, projection());

    const std::vector<Entity *> &entities = _scene->entities();

    _shader->bind();
    renderMeshes(*_shader, [&](size_t i) { return _selection.count(entities[i]) == 0; });
    _wireframeShader->bind();
    _wireframeShader->uniformF("Resolution", width(), height());
    renderMeshes(*_wireframeShader, [&](size_t i) { return _selection.count(entities[i]) == 1; });

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!_selection.empty())
        _gizmo.draw();

    if (_selectionState.selecting) {
        float startX = min(_selectionState.endX, _selectionState.startX);
        float startY = min(_selectionState.endY, _selectionState.startY);
        float w = std::abs(_selectionState.endX - _selectionState.startX);
        float h = std::abs(_selectionState.endY - _selectionState.startY);
        if (w != 0 && h != 0) {
            ShapePainter painter;
            painter.setColor(Vec3f(0.0f));
            painter.drawRectStipple(Vec2f(startX, startY), Vec2f(w, h), 20.0f, 2.0f);
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void PreviewWindow::resizeGL(int width, int height)
{
    _gizmo.resize(width, height);
    _gizmo.setProjection(projection());
    if (!_screenBuffer || width > _screenBuffer->width() || height > _screenBuffer->height()) {
        _screenBuffer.reset();
        _depthBuffer.reset();
    }
    updateGL();
}

void PreviewWindow::mouseMoveEvent(QMouseEvent *event)
{
    handleMouse(event);
}

void PreviewWindow::mousePressEvent(QMouseEvent *event)
{
    if (!handleMouse(event)) {
        if (event->button() == Qt::RightButton)
            showContextMenu();
    }
}

void PreviewWindow::mouseReleaseEvent(QMouseEvent *event)
{
    handleMouse(event);
}

void PreviewWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Control:
        _gizmo.setSnapToGrid(true);
        break;
    case Qt::Key_Escape:
        _gizmo.abortTransform();
        break;
    }
    QGLWidget::keyPressEvent(event);
    updateGL();
}

void PreviewWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control)
        _gizmo.setSnapToGrid(false);
    QGLWidget::keyPressEvent(event);
}

void PreviewWindow::showContextMenu()
{
    if (!_scene)
        return;

    std::array<QAction *, 6> actions;
    QMenu menu;
    actions[0] = menu.addAction("Add Cone light");
    actions[1] = menu.addAction("Add Directional light");
    actions[2] = menu.addAction("Add Environment light");
    actions[3] = menu.addAction("Add Point light");
    actions[4] = menu.addAction("Add Quad light");
    actions[5] = menu.addAction("Add Sphere light");

    QAction* selectedItem = menu.exec(QCursor::pos());
    for (size_t i = 0; i < actions.size(); ++i) {
        if (selectedItem == actions[i]) {
            switch (i) {
            case 0: _scene->addLight(std::make_shared<ConeLight>(Angle::degToRad(5.0f), Vec3f(40.0f))); break;
            case 1: _scene->addLight(std::make_shared<DirectionalLight>(Vec3f(10.0f))); break;
            case 2: _scene->addLight(std::make_shared<EnvironmentLight>(Angle::degToRad(5.0f), Vec3f(400.0f))); break;
            case 3: _scene->addLight(std::make_shared<      PointLight>(Vec3f(10.0f))); break;
            case 4: _scene->addLight(std::make_shared<       QuadLight>(Vec3f(10.0f))); break;
            case 5: _scene->addLight(std::make_shared<     SphereLight>(Vec3f(10.0f))); break;
            }

            Mat4f tform(Mat4f::translate(_scene->camera()->lookAt()));
            _scene->lights().back()->setTransform(tform*_scene->lights().back()->transform());

            _selection.clear();
            _selection.insert(_scene->lights().back().get());
            updateFixedTransform();

            _rebuildMeshes = true;
            updateGL();
            return;
        }
    }

}

void PreviewWindow::sceneChanged()
{
    _selection.clear();
    _scene = _parent.scene();

    if (_scene) {
        _controls.set(_scene->camera()->pos(), _scene->camera()->lookAt(), _scene->camera()->up());
        _rebuildMeshes = true;
    }

    updateGL();
}

}
