#include <QtGui>
#include <iostream>

#include "MainWindow.hpp"
#include "PreviewWindow.hpp"
#include "RenderWindow.hpp"

#include "io/FileUtils.hpp"

#include "cameras/PinholeCamera.hpp"

namespace Tungsten
{

MainWindow::MainWindow()
: QMainWindow()
{
    _scene.reset();
    QGLFormat qglFormat;
    qglFormat.setVersion(4, 2);
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
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Open file...",
        QString::fromStdString(FileUtils::getCurrentDir()),
        "Scene files (*.json)"
    );

    if (!file.isEmpty()) {
        openScene(file);
    }
}

void MainWindow::openScene(const QString &path)
{
    Scene *newScene = Scene::load(path.toStdString());

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
            Scene::save(_scene->path(), *_scene, true);
    }
}

void MainWindow::saveSceneAs()
{
    if (_scene) {
        QString file = QFileDialog::getSaveFileName(
            nullptr,
            "Save file as...",
            QString::fromStdString(FileUtils::getCurrentDir()),
            "Scene files (*.json)"
        );

        if (!file.isEmpty()) {
            _scene->setPath(file.toStdString());
            saveScene();
        }
    }
}

}
