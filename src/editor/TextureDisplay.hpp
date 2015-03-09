#ifndef TEXTUREDISPLAY_HPP_
#define TEXTUREDISPLAY_HPP_

#include <QLabel>

namespace Tungsten {

class Texture;

class TextureDisplay : public QLabel
{
    Q_OBJECT

    int _w, _h;
    Texture *_tex;

    QImage _image;

    void rebuildImage();

public:
    TextureDisplay(int w, int h, QWidget *parent = nullptr);

public slots:
    void changeTexture(Texture *tex);
};

}



#endif /* TEXTUREDISPLAY_HPP_ */
