#ifndef MEDIUMPROPERTY_HPP_
#define MEDIUMPROPERTY_HPP_

#include "Property.hpp"

#include <functional>
#include <memory>

class QPushButton;
class QComboBox;
class QLabel;

namespace Tungsten {

class PropertyForm;
class Medium;
class Scene;

class MediumProperty : public Property
{
    Q_OBJECT

    std::shared_ptr<Medium> _value;
    std::function<bool(std::shared_ptr<Medium> &)> _setter;
    Scene *_scene;

    QLabel *_nameLabel;
    QComboBox *_mediumSelector;

    void buildMediumList();

public:
    MediumProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::shared_ptr<Medium> value,
            std::function<bool(std::shared_ptr<Medium> &)> setter, Scene *scene);

    virtual void setVisible(bool visible) override;
};

}

#endif /* MEDIUMPROPERTY_HPP_ */
