#ifndef PRIMITIVEPROPERTIES_HPP_
#define PRIMITIVEPROPERTIES_HPP_

#include <unordered_set>
#include <QWidget>
#include <memory>

namespace Tungsten {

class PropertySheet;
class Primitive;
class Scene;

class PrimitiveProperties : public QWidget
{
    Q_OBJECT

    Scene *_scene;
    std::unordered_set<Primitive *> &_selection;

    void fillPropertySheet(PropertySheet *sheet, Primitive *p);

public:
    PrimitiveProperties(QWidget *proxyParent, Scene *scene, std::unordered_set<Primitive *> &selection);

signals:
    void primitiveNameChange(Primitive *p);
    void triggerRedraw();
};

}



#endif /* PRIMITIVEPROPERTIES_HPP_ */
