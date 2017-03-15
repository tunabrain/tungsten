#ifndef BOXFILTER_HPP_
#define BOXFILTER_HPP_

#include "math/Box.hpp"

#include "Pixmap.hpp"

namespace Tungsten {

template<typename Texel>
void boxFilterSlow(const Pixmap<Texel> &src, Pixmap<Texel> &result, int R, Box2i subImage = Box2i())
{
    if (subImage.empty())
        subImage = Box2i(Vec2i(0), Vec2i(src.w(), src.h()));

    int x0 = subImage.min().x(), x1 = subImage.max().x();
    int y0 = subImage.min().y(), y1 = subImage.max().y();

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            Texel sum(0.0f);
            int pixelCount = 0;
            for (int dy = -R; dy <= R; ++dy) {
                for (int dx = -R; dx <= R; ++dx) {
                    int xp = x + dx;
                    int yp = y + dy;
                    if (xp >= x0 && xp < x1 && yp >= y0 && yp < y1) {
                        sum += src(xp, yp);
                        pixelCount++;
                    }
                }
            }

            result(x, y) = sum/pixelCount;
        }
    }
}

template<typename Texel>
void boxFilter(const Pixmap<Texel> &src, Pixmap<Texel> &tmp, Pixmap<Texel> &result, int R, Box2i subImage = Box2i())
{
    if (subImage.empty())
        subImage = Box2i(Vec2i(0), Vec2i(src.w(), src.h()));

    int x0 = subImage.min().x(), x1 = subImage.max().x();
    int y0 = subImage.min().y(), y1 = subImage.max().y();

    if (x1 - x0 < 2*R || y1 - y0 < 2*R) {
        boxFilterSlow(src, result, R, subImage);
        return;
    }

    float factor = 1.0f/(2*R + 1);

    for (int y = y0; y < y1; ++y) {
        Texel sumL(0.0f);
        Texel sumR(0.0f);
        for (int x = 0; x < 2*R; ++x) {
            sumL += src(x0 + x, y);
            sumR += src(x1 - 1 - x, y);
            if (x >= R) {
                tmp(x0 + x - R      , y) = sumL/(x + 1);
                tmp(x1 - 1 - (x - R), y) = sumR/(x + 1);
            }
        }
        for (int x = x0 + R; x < x1 - R; ++x) {
            sumL += src(x + R, y);
            tmp(x, y) = sumL*factor;
            sumL -= src(x - R, y);
        }
    }

    for (int x = x0; x < x1; ++x) {
        Texel sumL(0.0f);
        Texel sumR(0.0f);
        for (int y = 0; y < 2*R; ++y) {
            sumL += tmp(x, y0 + y);
            sumR += tmp(x, y1 - 1 - y);
            if (y >= R) {
                result(x, y0 + y - R)       = sumL/(y + 1);
                result(x, y1 - 1 - (y - R)) = sumR/(y + 1);
            }
        }
        for (int y = y0 + R; y < y1 - R; ++y) {
            sumL += tmp(x, y + R);
            result(x, y) = sumL*factor;
            sumL -= tmp(x, y - R);
        }
    }
}

}

#endif /* BOXFILTER_HPP_ */
