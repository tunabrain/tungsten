#ifndef MAINWINDOW_HPP_
#define MAINWINDOW_HPP_

#include <QMainWindow>
#include <memory>

#include "io/Scene.hpp"

class QStackedWidget;
class QLabel;

namespace Tungsten {

class RenderWindow;
class PreviewWindow;
class Scene;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    RenderWindow *_renderWindow;
    PreviewWindow *_previewWindow;
    QStackedWidget *_stackWidget;
    std::unique_ptr<Scene> _scene;

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
};

}

#endif /* MAINWINDOW_HPP_ */
