#include <QtGui>
#include <iostream>

#include "MainWindow.hpp"
#include "PreviewWindow.hpp"
#include "RenderWindow.hpp"

#include "io/FileUtils.hpp"

#include "cameras/PinholeCamera.hpp"

namespace Tungsten {

MainWindow::MainWindow()
: QMainWindow()
{
    _scene.reset();

    if (!QGLFormat::hasOpenGL()) {
        QMessageBox::critical(this,
                "No OpenGL Support",
                "This system does not appear to support OpenGL.\n\n"
                "The Tungsten scene editor requires OpenGL "
                "to work properly. The editor will now terminate.\n\n"
                "Please install any available updates for your graphics card driver and try again");
        std::exit(0);
    }

    QGLFormat qglFormat;
    qglFormat.setVersion(3, 2);
    qglFormat.setProfile(QGLFormat::CoreProfile);
    qglFormat.setAlpha(true);
    qglFormat.setSampleBuffers(true);
    qglFormat.setSamples(16);

    _stackWidget = new QStackedWidget(this);

     _renderWindow = new  RenderWindow(_stackWidget, this);
    _previewWindow = new PreviewWindow(_stackWidget, this, qglFormat);

    _stackWidget->addWidget(_renderWindow);
    _stackWidget->addWidget(_previewWindow);

    setCentralWidget(_stackWidget);

    _previewWindow->addStatusWidgets(statusBar());
     _renderWindow->addStatusWidgets(statusBar());

    connect(this, SIGNAL(sceneChanged()), _previewWindow, SLOT(sceneChanged()));
    connect(this, SIGNAL(sceneChanged()),  _renderWindow, SLOT(sceneChanged()));

    showPreview(true);

    QMenu *fileMenu = new QMenu("&File");
    fileMenu->addAction("New",          this, SLOT(newScene()),  QKeySequence("Ctrl+N"));
    fileMenu->addAction("Open File...", this, SLOT(openScene()), QKeySequence("Ctrl+O"));
    fileMenu->addAction("Reload File...", this, SLOT(reloadScene()), QKeySequence("Shift+R"));
    fileMenu->addSeparator();
    fileMenu->addAction("Close", this, SLOT(closeScene()), QKeySequence("Ctrl+W"));
    fileMenu->addSeparator();
    fileMenu->addAction("Save",       this, SLOT(saveScene()),   QKeySequence("Ctrl+S"));
    fileMenu->addAction("Save as...", this, SLOT(saveSceneAs()), QKeySequence("Ctrl+Shift+S"));
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, SLOT(close()));

    QMenuBar *menuBar = new QMenuBar();
    menuBar->addMenu(fileMenu);

    setMenuBar(menuBar);

    newScene();
}

void MainWindow::togglePreview()
{
    showPreview(!_showPreview);
}

void MainWindow::showPreview(bool v)
{
    _showPreview = v;
    _stackWidget->setCurrentIndex(v);
}

void MainWindow::newScene()
{
    _scene.reset(new Scene());
    _scene->setCamera(new PinholeCamera());
    emit sceneChanged();
}

void MainWindow::openScene()
{
    std::string dir = (!_scene || _scene->path().empty()) ?
            FileUtils::getCurrentDir() :
            _scene->path();
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Open file...",
        QString::fromStdString(dir),
        "Scene files (*.json)"
    );

    if (!file.isEmpty())
        openScene(file);
}

void MainWindow::reloadScene()
{
    if (_scene)
        openScene(QString::fromStdString(_scene->path()));
}

void MainWindow::openScene(const QString &path)
{
    Scene *newScene = nullptr;
    try {
        newScene = Scene::load(path.toStdString());
        newScene->loadResources();
    } catch (const std::runtime_error &e) {
        QMessageBox::warning(
            this,
            "Loading scene failed",
            QString::fromStdString(tfm::format("Encountered an error while loading scene file:\n\n%s", e.what()))
        );
    }

    if (newScene) {
        _scene.reset(newScene);
        emit sceneChanged();
    }
}

void MainWindow::closeScene()
{
    _scene.reset();
    emit sceneChanged();
}

void MainWindow::saveScene()
{
    if (_scene) {
        if (_scene->path().empty())
            saveSceneAs();
        else
            Scene::save(_scene->path(), *_scene);
    }
}

void MainWindow::saveSceneAs()
{
    if (_scene) {
        std::string dir = _scene->path().empty() ?
                FileUtils::getCurrentDir() :
                _scene->path();
        QString file = QFileDialog::getSaveFileName(
            nullptr,
            "Save file as...",
            QString::fromStdString(dir),
            "Scene files (*.json)"
        );

        if (!file.isEmpty()) {
            _scene->setPath(file.toStdString());
            saveScene();
        }
    }
}

}
