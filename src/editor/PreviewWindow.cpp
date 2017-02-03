
#include <QtCore/QtCore>
#include <QtWidgets>

#include "PreviewWindow.hpp"
#include "ShapePainter.hpp"
#include "MainWindow.hpp"
#include "CoreProfileContext.hpp"

#include "primitives/TriangleMesh.hpp"

#include "cameras/Camera.hpp"

#include "opengl/OpenGL.hpp"
#include "opengl/MatrixStack.hpp"

#include "io/FileUtils.hpp"
#include "io/ObjLoader.hpp"

namespace Tungsten {

static const std::array<MouseConsumers, 3> DefaultPriorities{{GizmoConsumer, CameraConsumer, SelectionConsumer}};

using namespace GL;

GlMesh::GlMesh(const TriangleMesh &src)
: _vertexBuffer(src.verts().size()),
  _indexBuffer(ELEMENT_ARRAY_BUFFER, src.tris().size()*3*sizeof(uint32))
{
    _vertexBuffer.setStandardAttributes(VBO_ATT_POSITION | VBO_ATT_NORMAL | VBO_ATT_TEXCOORD0);
    _vertexBuffer.initBuffer();

    _vertexBuffer.bind();
    VboVertex *verts = _vertexBuffer.map<VboVertex>();
    for (const Vertex &v : src.verts()) {
        VboVertex vert = {v.pos(), v.normal(), v.uv()};
        *(verts++) = vert;
    }
    _vertexBuffer.unmap();

    _indexBuffer.bind();
    VboTriangle *tris = _indexBuffer.map<VboTriangle>();
    for (const TriangleI &t : src.tris()) {
        VboTriangle tri = {t.v0, t.v1, t.v2};
        *(tris++) = tri;
    }
    _indexBuffer.unmap();
}

void GlMesh::draw(Shader &shader)
{
    _vertexBuffer.drawIndexed(_indexBuffer, shader, GL_TRIANGLES);
}

PreviewWindow::PreviewWindow(QWidget */*proxyParent*/, MainWindow *parent)
: QGLWidget(parent),
  _parent(*parent),
  _scene(parent->scene()),
  _selection(parent->selection()),
  _mousePriorities(DefaultPriorities),
  _rebuildMeshes(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    QGLFormat fmt;
    fmt.setVersion(3, 2);
    fmt.setProfile(QGLFormat::CoreProfile);
    fmt.setAlphaBufferSize(8);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);

    new QShortcut(QKeySequence("A"), this, SLOT(toggleSelectAll()));
    new QShortcut(QKeySequence("G"), this, SLOT(grabGizmo()));
    new QShortcut(QKeySequence("Ctrl+C"), this, SLOT(recomputeCentroids()));
    new QShortcut(QKeySequence("Ctrl+R"), this, SLOT(computeSmoothNormals()));
    new QShortcut(QKeySequence("Ctrl+Shift+R"), this, SLOT(computeHardNormals()));
    new QShortcut(QKeySequence("Ctrl+F"), this, SLOT(freezeTransforms()));
    new QShortcut(QKeySequence("Ctrl+D"), this, SLOT(duplicateSelection()));
    new QShortcut(QKeySequence("Ctrl+A"), this, SLOT(addModel()));
    new QShortcut(QKeySequence("Delete"), this, SLOT(deleteSelection()));
    new QShortcut(QKeySequence("Ctrl+Tab"), this, SLOT(togglePreview()));
    new QShortcut(QKeySequence("["), this, SLOT(resetCamera()));

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
    connect(&_gizmo, SIGNAL(redraw()), this, SLOT(update()));
    connect(&_gizmo, SIGNAL(transformFinished(Mat4f)), this, SLOT(transformFinished(Mat4f)));
}

void PreviewWindow::addStatusWidgets(QStatusBar *statusBar)
{
    statusBar->addPermanentWidget(new QLabel(), 1);
}

void PreviewWindow::saveSceneData()
{
    for (Primitive *p : _dirtyPrimitives)
        p->saveResources();
    _dirtyPrimitives.clear();
}

Mat4f PreviewWindow::projection() const
{
    float nearPlane = Near;
    float farPlane = Far;
    float radius = (_scene->camera()->pos() - _scene->camera()->lookAt()).length();
    if (radius*2.0f > farPlane) {
        farPlane = radius*2.0f;
        nearPlane = Near/Far*farPlane;
    }

    return Mat4f::perspective(Fov, width()/float(height()), nearPlane, farPlane);
}

void PreviewWindow::rebuildMeshMap()
{
    std::unordered_map<Primitive *, std::shared_ptr<GlMesh>> tmpMap = std::move(_meshes);
    _meshes.clear();

    if (!_scene)
        return;

    for (const std::shared_ptr<Primitive> &e : _scene->primitives()) {
        auto iter = tmpMap.find(e.get());

        if (iter == tmpMap.end())
            _meshes.insert(std::make_pair(e.get(), std::make_shared<GlMesh>(e->asTriangleMesh())));
        else
            _meshes.insert(std::make_pair(e.get(), iter->second));
    }
}

bool PreviewWindow::updateViewTransform(QMouseEvent *event)
{
    if (!_scene)
        return false;

    bool result = _controls.update(event, width(), height(), _scene->camera()->approximateFov());
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
        for (Primitive *e : _selection)
            center += e->transform()*Vec3f(0.0f);
        center /= _selection.size();

        _gizmo.setFixedTransform(Mat4f::translate(center));
    }
}

template<typename Predicate>
void PreviewWindow::renderMeshes(Shader &shader, Predicate predicate)
{
    const std::vector<std::shared_ptr<Primitive>> &primitives = _scene->primitives();
    for (size_t i = 0; i < primitives.size(); ++i) {
        if (!predicate(i))
            continue;
        MatrixStack::set(MODEL_STACK, primitives[i]->transform());
        if (_selection.count(primitives[i].get()) && _gizmo.transforming())
            MatrixStack::mulL(MODEL_STACK, _gizmo.deltaTransform());

        MatrixStack::setShaderMatrices(shader, MODELVIEWPROJECTION_FLAG | MODEL_FLAG | VIEW_FLAG);
        shader.uniformI("Smooth", primitives[i]->asTriangleMesh().smoothed());
        shader.uniformI("NoShading", primitives[i]->isEmissive());
        _meshes[primitives[i].get()]->draw(shader);
    }
}

void PreviewWindow::pickPrimitive()
{
    const std::vector<std::shared_ptr<Primitive>> &prims = _scene->primitives();

    std::shared_ptr<Camera> cam = _scene->camera();

    MatrixStack::set(VIEW_STACK, cam->transform());
    MatrixStack::set(PROJECTION_STACK, projection());

    _fbo->bind();
    _fbo->attachDepthBuffer(*_depthBuffer);
    _fbo->attachTexture(*_screenBuffer, 0);
    _fbo->selectAttachments(1);
    _fbo->setReadBuffer(RT_ATTACHMENT0);

    glf->glViewport(0, 0, width(), height());

    glf->glDisable(GL_MULTISAMPLE);
    glf->glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glf->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
    int w = clamp(std::abs(_selectionState.endX - _selectionState.startX), 1, width() - startX);
    int h = clamp(std::abs(_selectionState.endY - _selectionState.startY), 1, height() - startY);

    std::unordered_set<uint32> uniquePixels;
    std::vector<uint32> buffer(w*h);
    glf->glReadPixels(startX, height() - startY - h - 1, w, h, GL_RGBA, GL_UNSIGNED_BYTE, &buffer[0]);

    _fbo->unbind();
    glf->glEnable(GL_MULTISAMPLE);

    for (uint32 pixel : buffer) {
        if (pixel != 0xFFFFFFFF) {
            ASSERT(pixel < prims.size(), "Invalid prim id: %d", pixel);
            uniquePixels.insert(pixel);
        }
    }

    if (_selectionState.shiftDown) {
        if (w == 1 && h == 1 && !uniquePixels.empty()) {
            Primitive *prim = prims[*uniquePixels.begin()].get();
            if (_selection.count(prim))
                _selection.erase(prim);
            else
                _selection.insert(prim);
        } else {
            for (uint32 id : uniquePixels)
                _selection.insert(prims[id].get());
        }
    } else {
        _selection.clear();
        for (uint32 id : uniquePixels)
            _selection.insert(prims[id].get());
    }

    _selectionState = SelectionState();
    updateFixedTransform();

    emit selectionChanged();
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
            if (!_selection.empty())
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
    update();

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
        for (const std::shared_ptr<Primitive> &e : _scene->primitives())
            _selection.insert(e.get());
    }

    updateFixedTransform();
    update();

    emit selectionChanged();
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
    for (Primitive *e : _selection)
        e->setTransform(delta*e->transform());
    updateFixedTransform();
    update();
}

void PreviewWindow::recomputeCentroids()
{
    for (Primitive *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            Vec3f centroid(0.0f);
            for (const Vertex &v : m->verts())
                centroid += v.pos();
            centroid /= m->verts().size();
            for (Vertex &v : m->verts())
                v.pos() -= centroid;
            m->setTransform(m->transform()*Mat4f::translate(centroid));
            _dirtyPrimitives.insert(m);

            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    updateFixedTransform();
    update();
}

void PreviewWindow::computeHardNormals()
{
    for (Primitive *e : _selection)
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e))
            m->setSmoothed(false);
    update();
}

void PreviewWindow::computeSmoothNormals()
{
    for (Primitive *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            m->calcSmoothVertexNormals();
            m->setSmoothed(true);
            _dirtyPrimitives.insert(m);
            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    update();
}

void PreviewWindow::freezeTransforms()
{
    for (Primitive *e : _selection) {
        if (TriangleMesh *m = dynamic_cast<TriangleMesh *>(e)) {
            Mat4f tform = m->transform().stripTranslation();
            for (Vertex &v : m->verts())
                v.pos() = tform*v.pos();
            _dirtyPrimitives.insert(m);
            m->setTransform(m->transform().extractTranslation());

            _meshes.erase(m);
        }
    }

    _rebuildMeshes = true;
    updateFixedTransform();
    update();
}

void PreviewWindow::duplicateSelection()
{
    if (!_scene)
        return;

    std::unordered_set<Primitive *> newSelection;
    for (Primitive *e : _selection) {
        Primitive *newE = e->clone();
        newSelection.insert(newE);
        _scene->addPrimitive(std::shared_ptr<Primitive>(newE));
    }

    _selection = std::move(newSelection);

    rebuildMeshMap();
    updateFixedTransform();
    update();

    emit selectionChanged();
    emit primitiveListChanged();
}

void PreviewWindow::deleteSelection()
{
    if (!_scene)
        return;
    _scene->deletePrimitives(_selection);
    _selection.clear();
    _gizmo.abortTransform();
    rebuildMeshMap();
    update();

    emit selectionChanged();
    emit primitiveListChanged();
}

void PreviewWindow::addModel()
{
    if (!_scene)
        return;

    Path dir = _scene->path().empty() ?
        FileUtils::getCurrentDir() :
        _scene->path();
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Open file...",
        QString::fromStdString(dir.absolute().asString()),
        "Mesh files (*.obj *.json)"
    );

    if (!file.isEmpty()) {
        Path p(file.toStdString());

        Scene *scene = nullptr;
        try {
            if (p.testExtension("obj")) {
                scene = ObjLoader::load(p, _scene->textureCache());
            } else if (p.testExtension("json")) {
                scene = Scene::load(p, _scene->textureCache());
                if (scene)
                    scene->loadResources();
            }

        } catch (const std::runtime_error &e) {
            QMessageBox::warning(
                this,
                "Loading model failed",
                QString::fromStdString(tfm::format("Encountered an error while loading model:\n\n%s", e.what()))
            );
        }
        if (!scene)
            return;

        size_t  primTail = _scene->primitives().size();

        Mat4f tform = Mat4f::translate(_scene->camera()->lookAt() - scene->camera()->lookAt());

        _scene->merge(std::move(*scene));
        delete scene;

        _selection.clear();
        for (size_t i = primTail; i < _scene->primitives().size(); ++i) {
            _scene->primitives()[i]->setTransform(tform*_scene->primitives()[i]->transform());
            _selection.insert(_scene->primitives()[i].get());
        }

        rebuildMeshMap();
        updateFixedTransform();
        update();

        emit selectionChanged();
        emit primitiveListChanged();
    }
}

void PreviewWindow::togglePreview()
{
    if (!_gizmo.transforming())
        _parent.togglePreview();
}

void PreviewWindow::resetCamera()
{
    _controls.set(_initialPos, _initialTarget, _initialUp);
    _scene->camera()->setTransform(_controls.globalPos(), _controls.lookAt(), _controls.up());
    _gizmo.setView(_scene->camera()->transform());
    update();
}

void PreviewWindow::initializeGL()
{
    GL::initOpenGL();

    if (!(qMakePair(format().majorVersion(), format().minorVersion()) >= qMakePair(3, 2)) ||
        !(QOpenGLContext::currentContext()->isValid())) {
        QMessageBox::critical(this,
                "No OpenGL Support",
                "This system does not appear to support OpenGL 3.2.\n\n"
                "The Tungsten scene editor requires OpenGL version 3.2 or higher "
                "to work properly. The editor will now terminate.\n\n"
                "Please install any available updates for your graphics card driver and try again");
        std::exit(0);
    }

    GLuint vao;
    glf->glGenVertexArrays(1, &vao);
    glf->glBindVertexArray(vao);

    glf->glEnable(GL_DEPTH_TEST);
    glf->glDepthFunc(GL_LEQUAL);

    _fbo.reset(new RenderTarget());

    Path shaderBasePath = FileUtils::getDataPath()/"shaders/";

    _shader.reset(
        new Shader(shaderBasePath, "Preamble.txt", "MeshPreview.vert", "MeshPreview.geom", "MeshPreview.frag", 1));
    _solidShader.reset(
        new Shader(shaderBasePath, "Preamble.txt", "SolidMesh.vert", "", "SolidMesh.frag", 1));
    _wireframeShader.reset(
        new Shader(shaderBasePath, "Preamble.txt", "Wireframe.vert", "Wireframe.geom", "Wireframe.frag", 1));
}

void PreviewWindow::drawBackgroundGradient()
{
    float w = width();
    float h = height();

    glf->glDepthMask(GL_FALSE);

    {
        ShapePainter painter;
        painter.begin(ShapePainter::MODE_QUADS);
        painter.setColor(Vec3f(0.25f));
        painter.addVertex(Vec2f(0.0f, 0.0f));
        painter.addVertex(Vec2f(   w, 0.0f));
        painter.setColor(Vec3f(0.3f));
        painter.addVertex(Vec2f(   w,    h));
        painter.addVertex(Vec2f(0.0f,    h));
    }

    glf->glDepthMask(GL_TRUE);
}

void PreviewWindow::drawGrid()
{
    Mat4f proj;
    MatrixStack::set(MODEL_STACK, Mat4f());
    MatrixStack::get(MODELVIEWPROJECTION_STACK, proj);

    float radius = (_scene->camera()->pos() - _scene->camera()->lookAt()).length();
    int exponent = max(int(std::log10(radius) + 0.5f), -10);
    float scale = std::pow(10.0f, exponent);

    ShapePainter painter(proj);
    for (int i = -100; i <= 100; ++i) {
        painter.setColor(Vec3f((i % 10) == 0 ? 0.0f : 0.2f));
        float s = i*0.1f*scale;
        float t = 10.0f*scale;
        if (i == 0) {
            painter.drawLine(Vec3f(s, 0.0f, -t), Vec3f(s, 0.0f, -scale));
            painter.drawLine(Vec3f(s, 0.0f, scale), Vec3f(s, 0.0f, t));
            painter.drawLine(Vec3f(-t, 0.0f, s), Vec3f(-scale, 0.0f, s));
            painter.drawLine(Vec3f(scale, 0.0f, s), Vec3f(t, 0.0f, s));
        } else {
            painter.drawLine(Vec3f(s, 0.0f, -t), Vec3f(s, 0.0f, t));
            painter.drawLine(Vec3f(-t, 0.0f, s), Vec3f(t, 0.0f, s));
        }
    }

    painter.setColor(Vec3f(1.0f, 0.0f, 0.0f));
    painter.drawLine(Vec3f(-scale, 0.0f, 0.0f), Vec3f(scale, 0.0f, 0.0f));
    painter.setColor(Vec3f(0.0f, 0.0f, 1.0f));
    painter.drawLine(Vec3f(0.0f, 0.0f, -scale), Vec3f(0.0f, 0.0f, scale));
}

void PreviewWindow::paintGL()
{
    if (_rebuildMeshes) {
        _rebuildMeshes = false;
        rebuildMeshMap();
    }

    glf->glViewport(0, 0, width(), height());
    RenderTarget::resetViewport();

    if (!_screenBuffer && width() > 0 && height() > 0) {
        _screenBuffer.reset(new GL::Texture(TEXTURE_2D, width(), height()));
        _screenBuffer->setFormat(TEXEL_FLOAT, 4, 1);
        _screenBuffer->init();
        _depthBuffer.reset(new GL::Texture(TEXTURE_2D, width(), height()));
        _depthBuffer->setFormat(TEXEL_DEPTH, 1, 3);
        _depthBuffer->init();
    }

    if (_selectionState.finished && _scene) {
        pickPrimitive();
        makeCurrent();
    }

    glf->glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glf->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawBackgroundGradient();

    if (!_scene)
        return;

    std::shared_ptr<Camera> cam = _scene->camera();

    MatrixStack::set(VIEW_STACK, cam->transform());
    MatrixStack::set(PROJECTION_STACK, projection());

    drawGrid();

    glf->glEnable(GL_FRAMEBUFFER_SRGB);

    const std::vector<std::shared_ptr<Primitive>> &prims = _scene->primitives();

    _shader->bind();
    renderMeshes(*_shader, [&](size_t i) { return _selection.count(prims[i].get()) == 0; });
    _wireframeShader->bind();
    _wireframeShader->uniformF("Resolution", width(), height());
    renderMeshes(*_wireframeShader, [&](size_t i) { return _selection.count(prims[i].get()) == 1; });

    glf->glDisable(GL_DEPTH_TEST);
    glf->glEnable(GL_BLEND);
    glf->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    glf->glDisable(GL_BLEND);
    glf->glEnable(GL_DEPTH_TEST);

    glf->glDisable(GL_FRAMEBUFFER_SRGB);
}

void PreviewWindow::resizeGL(int width, int height)
{
    _gizmo.resize(width, height);
    _gizmo.setProjection(projection());
    if (!_screenBuffer || width > _screenBuffer->width() || height > _screenBuffer->height()) {
        _screenBuffer.reset();
        _depthBuffer.reset();
    }
    update();
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
    update();
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

/*
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
            update();
            return;
        }
    }
*/
}

void PreviewWindow::sceneChanged()
{
    _scene = _parent.scene();

    if (_scene) {
        _initialPos = _scene->camera()->pos();
        _initialTarget = _scene->camera()->lookAt();
        _initialUp = _scene->camera()->up();
        _controls.set(_scene->camera()->pos(), _scene->camera()->lookAt(), _scene->camera()->up());
        _rebuildMeshes = true;
    }

    update();
}

void PreviewWindow::changeSelection()
{
    updateFixedTransform();
    update();
}

}
