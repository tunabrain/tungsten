#include "PropertyWindow.hpp"
#include "VerticalScrollArea.hpp"
#include "PreviewWindow.hpp"
#include "MainWindow.hpp"

#include "properties/PrimitiveProperties.hpp"
#include "properties/BsdfProperties.hpp"

#include "io/Scene.hpp"

#include <QtWidgets>

namespace Tungsten {

PropertyWindow::PropertyWindow(QWidget *proxyParent, MainWindow *parent)
: QSplitter(proxyParent),
  _parent(*parent),
  _scene(nullptr),
  _selection(parent->selection()),
  _openTab(0)
{
    _sceneTree = new QTreeWidget(this);
    _sceneTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _sceneTree->setHeaderHidden(true);

    _propertyTabs = new QTabWidget(this);

    addWidget(_sceneTree);
    addWidget(_propertyTabs);

    setOrientation(Qt::Vertical);
    setStretchFactor(0, 0);
    setStretchFactor(1, 1);

    setFocusPolicy(Qt::StrongFocus);

    connect(_sceneTree, SIGNAL(itemSelectionChanged()), this, SLOT(treeSelectionChanged()));
    connect(_propertyTabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
}

void PropertyWindow::populateSceneTree()
{
    _sceneTree->clear();
    _primToItem.clear();
    _itemToPrim.clear();

    if (!_scene)
        return;

    std::vector<Primitive *> infinites, finites;
    for (const auto &p : _scene->primitives()) {
        if (p->isInfinite())
            infinites.push_back(p.get());
        else
            finites.push_back(p.get());
    }

    _sceneTree->setColumnCount(1);
    _sceneTree->setHeaderLabel("Name");

    if (!infinites.empty()) {
        QTreeWidgetItem *root = new QTreeWidgetItem(_sceneTree, QStringList("Infinites"));
        _sceneTree->addTopLevelItem(root);
        for (Primitive *p : infinites) {
            QString name = p->name().empty() ? "<unnamed>" : p->name().c_str();
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
            _primToItem.insert(std::make_pair(p, item));
            _itemToPrim.insert(std::make_pair(item, p));
            root->addChild(item);
        }
    }
    if (!finites.empty()) {
        QTreeWidgetItem *root = new QTreeWidgetItem(_sceneTree, QStringList("Finites"));
        _sceneTree->addTopLevelItem(root);
        for (Primitive *p : finites) {
            QString name = p->name().empty() ? "<unnamed>" : p->name().c_str();
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
            _primToItem.insert(std::make_pair(p, item));
            _itemToPrim.insert(std::make_pair(item, p));
            root->addChild(item);
        }
    }

    _sceneTree->expandAll();
}

void PropertyWindow::rebuildTabs()
{
    _propertyTabs->blockSignals(true);
    while (_propertyTabs->count()) {
        QWidget *widget = _propertyTabs->widget(0);
        _propertyTabs->removeTab(0);
        delete widget;
    }
    if (_selection.empty())
        return;

    PrimitiveProperties *primProps = new PrimitiveProperties(_propertyTabs, _scene, _selection);
    connect(primProps, SIGNAL(primitiveNameChange(Primitive *)), this, SLOT(changePrimitiveName(Primitive *)));
    connect(primProps, SIGNAL(triggerRedraw()), _parent.previewWindow(), SLOT(update()));
    _propertyTabs->addTab(primProps, "Primitive");

    VerticalScrollArea *scrollArea = new VerticalScrollArea(_propertyTabs);
    BsdfProperties *bsdfProps = new BsdfProperties(scrollArea, _scene, _selection);
    connect(bsdfProps, SIGNAL(triggerRedraw()), _parent.previewWindow(), SLOT(update()));

    scrollArea->setWidget(bsdfProps);
    _propertyTabs->addTab(scrollArea, "Material");
    if (_openTab < _propertyTabs->count())
        _propertyTabs->setCurrentIndex(_openTab);
    else
        _openTab = 0;

    _propertyTabs->blockSignals(false);
}

void PropertyWindow::treeSelectionChanged()
{
    if (!_scene)
        return;

    _selection.clear();
    for (QTreeWidgetItem *i : _sceneTree->selectedItems()) {
        auto iter = _itemToPrim.find(i);
        if (iter != _itemToPrim.end())
            _selection.insert(iter->second);
    }

    rebuildTabs();

    emit selectionChanged();
}

void PropertyWindow::primitiveListChanged()
{
    populateSceneTree();
}

void PropertyWindow::tabChanged(int idx)
{
    _openTab = idx;
}

void PropertyWindow::sceneChanged()
{
    _scene = _parent.scene();
    populateSceneTree();

    _propertyTabs->clear();
}

void PropertyWindow::changeSelection()
{
    _sceneTree->blockSignals(true);
    _sceneTree->clearSelection();
    for (const Primitive *p : _selection) {
        auto iter = _primToItem.find(p);
        if (iter != _primToItem.end())
            iter->second->setSelected(true);
    }
    _sceneTree->blockSignals(false);

    rebuildTabs();
}

void PropertyWindow::changePrimitiveName(Primitive *p)
{
    auto iter = _primToItem.find(p);
    if (iter != _primToItem.end())
        iter->second->setText(0, p->name().empty() ? "<unnamed>" : p->name().c_str());
}

}
