#ifndef PROPERTYWINDOW_HPP_
#define PROPERTYWINDOW_HPP_

#include <unordered_map>
#include <unordered_set>
#include <QSplitter>
#include <memory>

class QTreeWidgetItem;
class QTreeWidget;
class QTabWidget;

namespace Tungsten {

class PrimitiveProperties;
class MainWindow;
class Primitive;
class Scene;

class PropertyWindow : public QSplitter
{
    Q_OBJECT

    MainWindow &_parent;
    Scene *_scene;
    std::unordered_set<Primitive *> &_selection;

    int _openTab;

    QTreeWidget *_sceneTree;
    QTabWidget *_propertyTabs;

    std::unordered_map<const Primitive *, QTreeWidgetItem *> _primToItem;
    std::unordered_map<QTreeWidgetItem *, Primitive *> _itemToPrim;


    void populateSceneTree();
    void rebuildTabs();

public:
    PropertyWindow(QWidget *proxyParent, MainWindow *parent);

private slots:
    void treeSelectionChanged();
    void primitiveListChanged();
    void tabChanged(int idx);

public slots:
    void sceneChanged();
    void changeSelection();
    void changePrimitiveName(Primitive *p);

signals:
    void selectionChanged();
};

}

#endif /* PROPERTYWINDOW_HPP_ */
