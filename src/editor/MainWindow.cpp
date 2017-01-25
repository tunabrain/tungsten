#include <QtWidgets>
#include <iostream>

#include "MainWindow.hpp"
#include "PropertyWindow.hpp"
#include "PreviewWindow.hpp"
#include "RenderWindow.hpp"
#include "LoadErrorDialog.hpp"

#include "io/FileUtils.hpp"

#include "cameras/PinholeCamera.hpp"

namespace Tungsten {

MainWindow::MainWindow()
: QMainWindow()
{
    _scene.reset();

    _windowSplit = new QSplitter(this);
    _stackWidget = new QStackedWidget(_windowSplit);

      _renderWindow = new   RenderWindow(_stackWidget, this);
     _previewWindow = new  PreviewWindow(_stackWidget, this);
    _propertyWindow = new PropertyWindow(_windowSplit, this);

    _stackWidget->addWidget(_renderWindow);
    _stackWidget->addWidget(_previewWindow);

    _windowSplit->addWidget(_stackWidget);
    _windowSplit->addWidget(_propertyWindow);
    _windowSplit->setStretchFactor(0, 1);
    _windowSplit->setStretchFactor(1, 0);

    setCentralWidget(_windowSplit);

    _previewWindow->addStatusWidgets(statusBar());
     _renderWindow->addStatusWidgets(statusBar());

    connect(this, SIGNAL(sceneChanged()),  _previewWindow, SLOT(sceneChanged()));
    connect(this, SIGNAL(sceneChanged()),   _renderWindow, SLOT(sceneChanged()));
    connect(this, SIGNAL(sceneChanged()), _propertyWindow, SLOT(sceneChanged()));

    connect( _previewWindow, SIGNAL(primitiveListChanged()), _propertyWindow, SLOT(primitiveListChanged()));
    connect( _previewWindow, SIGNAL(selectionChanged()), _propertyWindow, SLOT(changeSelection()));
    connect(_propertyWindow, SIGNAL(selectionChanged()),  _previewWindow, SLOT(changeSelection()));

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
    _selection.clear();
    _scene.reset(new Scene());
    _scene->setCamera(new PinholeCamera());
    emit sceneChanged();
}

void MainWindow::openScene()
{
    Path dir = (!_scene || _scene->path().empty()) ?
            FileUtils::getCurrentDir() :
            _scene->path();
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        "Open file...",
        QString::fromStdString(dir.absolute().asString()),
        "Scene files (*.json)"
    );

    if (!file.isEmpty())
        openScene(file);
}

void MainWindow::reloadScene()
{
    if (_scene)
        openScene(QString::fromStdString(_scene->path().absolute().asString()));
}


void MainWindow::openScene(const QString &path)
{
    Scene *newScene = nullptr;
    try {
        newScene = Scene::load(Path(path.toUtf8().data()));
    } catch (const JsonLoadException &e) {
        LoadErrorDialog *error = new LoadErrorDialog(this, e);
        error->exec();
    }

    if (newScene) {
        _selection.clear();
        _scene.reset(newScene);
        _scene->loadResources();
        emit sceneChanged();
    }
}

void MainWindow::closeScene()
{
    _selection.clear();
    _scene.reset();
    emit sceneChanged();
}

void MainWindow::saveScene()
{
    if (_scene) {
        if (_scene->path().empty()) {
            saveSceneAs();
        } else {
            Scene::save(_scene->path(), *_scene);
            _previewWindow->saveSceneData();
        }
    }
}

void MainWindow::saveSceneAs()
{
    if (_scene) {
        Path dir = _scene->path().empty() ?
                FileUtils::getCurrentDir() :
                _scene->path();
        QString file = QFileDialog::getSaveFileName(
            nullptr,
            "Save file as...",
            QString::fromStdString(dir.absolute().asString()),
            "Scene files (*.json)"
        );

        if (!file.isEmpty()) {
            _scene->setPath(Path(file.toStdString()));
            saveScene();
        }
    }
}

}
