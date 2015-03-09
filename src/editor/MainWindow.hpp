#ifndef MAINWINDOW_HPP_
#define MAINWINDOW_HPP_

#include <unordered_set>
#include <QMainWindow>
#include <memory>

#include "io/Scene.hpp"

class QStackedWidget;
class QSplitter;
class QLabel;

namespace Tungsten {

class PropertyWindow;
class PreviewWindow;
class RenderWindow;
class Scene;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    RenderWindow *_renderWindow;
    PreviewWindow *_previewWindow;
    PropertyWindow *_propertyWindow;
    QStackedWidget *_stackWidget;
    QSplitter *_windowSplit;
    std::unique_ptr<Scene> _scene;
    std::unordered_set<Primitive *> _selection;

    bool _showPreview;

private slots:
    void showPreview(bool v);

    void newScene();
    void openScene();
    void reloadScene();
    void closeScene();
    void saveScene();
    void saveSceneAs();

public slots:
    void togglePreview();
    void openScene(const QString &path);

signals:
    void sceneChanged();

public:
    MainWindow();

    Scene *scene()
    {
        return _scene.get();
    }

    std::unordered_set<Primitive *> &selection()
    {
        return _selection;
    }

    PreviewWindow *previewWindow()
    {
        return _previewWindow;
    }
};

}

#endif /* MAINWINDOW_HPP_ */
