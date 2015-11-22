#ifndef BSDFPROPERTIES_HPP_
#define BSDFPROPERTIES_HPP_

#include <unordered_set>
#include <QWidget>
#include <memory>

namespace Tungsten {

class PropertyForm;
class Primitive;
class Scene;

class BsdfProperties : public QWidget
{
    Q_OBJECT

    Scene *_scene;
    std::unordered_set<Primitive *> &_selection;

    void fillPropertySheet(PropertyForm *sheet, Primitive *p);

public:
    BsdfProperties(QWidget *proxyParent, Scene *scene, std::unordered_set<Primitive *> &selection);

signals:
    void bsdfNameChange(Primitive *p);
    void triggerRedraw();
};

}

#endif /* BSDFPROPERTIES_HPP_ */
