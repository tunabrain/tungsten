#include "ImageIO.hpp"

#include "FileUtils.hpp"

#include <lodepng/lodepng.h>
#include <stbi/stb_image.h>
#include <cstring>

static const int GammaCorrection[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,
      2,   2,   3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,
      6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  10,  11,  11,
     12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,
     19,  20,  21,  21,  22,  22,  23,  23,  24,  25,  25,  26,  27,  27,  28,  29,
     29,  30,  31,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,  40,  40,
     41,  42,  43,  44,  45,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
     55,  56,  57,  58,  59,  60,  61,  62,  63,  65,  66,  67,  68,  69,  70,  71,
     72,  73,  74,  75,  77,  78,  79,  80,  81,  82,  84,  85,  86,  87,  88,  90,
     91,  92,  93,  95,  96,  97,  99, 100, 101, 103, 104, 105, 107, 108, 109, 111,
    112, 114, 115, 117, 118, 119, 121, 122, 124, 125, 127, 128, 130, 131, 133, 135,
    136, 138, 139, 141, 142, 144, 146, 147, 149, 151, 152, 154, 156, 157, 159, 161,
    162, 164, 166, 168, 169, 171, 173, 175, 176, 178, 180, 182, 184, 186, 187, 189,
    191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
    223, 225, 227, 229, 231, 233, 235, 237, 239, 241, 244, 246, 248, 250, 252, 255
};

namespace Tungsten {

namespace ImageIO {

static int stbiReadCallback(void *user, char *data, int size)
{
    std::istream &in = *static_cast<std::istream *>(user);
    in.read(data, size);
    return in.gcount();
}
static void stbiSkipCallback(void *user, int n)
{
    std::istream &in = *static_cast<std::istream *>(user);
    in.seekg(n, std::ios_base::cur);
}
static int stbiEofCallback(void *user)
{
    std::istream &in = *static_cast<std::istream *>(user);
    return in.eof();
}
static const stbi_io_callbacks istreamCallback  = stbi_io_callbacks{
    &stbiReadCallback, &stbiSkipCallback, &stbiEofCallback
};

bool isHdr(const Path &path)
{
    if (path.testExtension("pfm"))
        return true;

    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return false;
    return stbi_is_hdr_from_callbacks(&istreamCallback, in.get());
}

template<typename T>
static T convertToScalar(TexelConversion request, T r, T g, T b, T a, bool haveAlpha)
{
    if (request == TexelConversion::REQUEST_AUTO)
        request = haveAlpha ? TexelConversion::REQUEST_ALPHA : TexelConversion::REQUEST_AVERAGE;

    switch(request) {
    case TexelConversion::REQUEST_AVERAGE: return (r + g + b)/3;
    case TexelConversion::REQUEST_RED:     return r;
    case TexelConversion::REQUEST_GREEN:   return g;
    case TexelConversion::REQUEST_BLUE:    return b;
    case TexelConversion::REQUEST_ALPHA:   return a;
    default:
        return T(0);
    }
}

static std::unique_ptr<float[]> loadPfm(const Path &path, TexelConversion request, int &w, int &h)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return nullptr;

    std::string ident;
    *in >> ident;
    int channels;
    if (ident == "Pf")
        channels = 1;
    else if (ident == "PF")
        channels = 3;
    else
        return nullptr;
    int targetChannels = (request == TexelConversion::REQUEST_RGB) ? 3 : 1;

    *in >> w >> h;
    double s;
    *in >> s;
    std::string tmp;
    std::getline(*in, tmp);

    std::unique_ptr<float[]> img(new float[w*h*channels]);
    for (int y = 0; y < h; ++y)
        in->read(reinterpret_cast<char *>(img.get() + (h - y - 1)*w*channels), w*channels*sizeof(float));

    if (channels == targetChannels)
        return std::move(img);

    std::unique_ptr<float[]> texels(new float[w*h*targetChannels]);

    if (targetChannels == 3)
        for (int i = 0; i < w*h; ++i)
            texels[i*3] = texels[i*3 + 1] = texels[i*3 + 2] = img[i];
    else
        for (int i = 0; i < w*h; ++i)
            texels[i] = convertToScalar(request, img[i*3], img[i*3 + 1], img[i*3 + 2], 1.0f, false);

    return std::move(texels);
}

std::unique_ptr<float[]> loadStbiHdr(const Path &path, TexelConversion request, int &w, int &h)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return nullptr;

    int channels;
    std::unique_ptr<float[], void(*)(void *)> img(stbi_loadf_from_callbacks(&istreamCallback, in.get(),
            &w, &h, &channels, 0), stbi_image_free);

    // We only expect Radiance HDR for now, which only has RGB support.
    if (!img || channels != 3)
        return nullptr;

    int targetChannels = (request == TexelConversion::REQUEST_RGB) ? 3 : 1;

    std::unique_ptr<float[]> texels(new float[w*h*targetChannels]);
    if (targetChannels == 3) {
        std::memcpy(texels.get(), img.get(), w*h*targetChannels*sizeof(float));
    } else {
        for (int i = 0; i < w*h; ++i)
            texels[i] = convertToScalar(request, img[i*3], img[i*3 + 1], img[i*3 + 2], 1.0f, false);
    }

    return std::move(texels);
}

std::unique_ptr<float[]> loadHdr(const Path &path, TexelConversion request, int &w, int &h)
{
    if (path.testExtension("pfm"))
        return std::move(loadPfm(path, request, w, h));
    else
        return std::move(loadStbiHdr(path, request, w, h));
}

typedef std::unique_ptr<uint8[], void(*)(void *)> DeletablePixels;
static inline DeletablePixels makeVoidPixels()
{
    return DeletablePixels((uint8 *)0, [](void *){});
}

DeletablePixels loadPng(const Path &path, int &w, int &h, int &channels)
{
    uint64 size = FileUtils::fileSize(path);
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (size == 0 || !in)
        return makeVoidPixels();

    std::unique_ptr<uint8[]> file(new uint8[size]);
    FileUtils::streamRead(in, file.get(), size);

    uint8 *dst = nullptr;
    uint32 uw, uh;
    lodepng_decode_memory(&dst, &uw, &uh, file.get(), size, LCT_RGBA, 8);
    if (!dst)
        return makeVoidPixels();

    w = uw;
    h = uh;
    channels = 4;

    return DeletablePixels(dst, free);
}

DeletablePixels loadStbi(const Path &path, int &w, int &h, int &channels)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return makeVoidPixels();

    return DeletablePixels(stbi_load_from_callbacks(&istreamCallback, in.get(),
            &w, &h, &channels, 4), stbi_image_free);
}

std::unique_ptr<uint8[]> loadLdr(const Path &path, TexelConversion request, int &w, int &h, bool gammaCorrect)
{
    int channels;
    DeletablePixels img(makeVoidPixels());
    if (path.testExtension("png"))
        img = loadPng(path, w, h, channels);
    else
        img = loadStbi(path, w, h, channels);

    if (!img)
        return nullptr;

    int targetChannels = (request == TexelConversion::REQUEST_RGB) ? 4 : 1;

    std::unique_ptr<uint8[]> texels(new uint8[w*h*targetChannels]);
    if (targetChannels == 4) {
        std::memcpy(texels.get(), img.get(), w*h*targetChannels*sizeof(uint8));

        if (gammaCorrect)
            for (int i = 0; i < w*h; ++i)
                for (int t = 0; t < 3; ++t)
                    texels[i*4 + t] = GammaCorrection[texels[i*4 + t]];
    } else {
        for (int i = 0; i < w*h; ++i)
            texels[i] = convertToScalar(request, int(img[i*4]), int(img[i*4 + 1]), int(img[i*4 + 2]),
                int(img[i*4 + 3]), channels == 4);
    }

    return std::move(texels);
}

bool savePfm(const Path &path, const float *img, int w, int h, int channels)
{
    if (channels != 1 && channels != 3)
        return false;

    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;

    *out << ((channels == 1) ? "Pf" : "PF") << '\n';
    *out << w << " " << h << '\n';
    *out << -1.0 << '\n';
    out->write(reinterpret_cast<const char *>(img), w*h*sizeof(float)*channels);

    return true;
}

bool savePng(const Path &path, const uint8 *img, int w, int h, int channels)
{
    if (channels <= 0 || channels > 4)
        return false;
    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;

    LodePNGColorType types[] = {LCT_GREY, LCT_GREY_ALPHA, LCT_RGB, LCT_RGBA};

    uint8 *encoded = nullptr;
    size_t encodedSize;
    if (lodepng_encode_memory(&encoded, &encodedSize, img, w, h, types[channels - 1], 8) != 0)
        return false;
    DeletablePixels data(encoded, free);

    FileUtils::streamWrite(out, data.get(), encodedSize);

    return true;
}

bool saveHdr(const Path &path, const float *img, int w, int h, int channels)
{
    if (path.testExtension("pfm"))
        return savePfm(path, img, w, h, channels);

    return false;
}

bool saveLdr(const Path &path, const uint8 *img, int w, int h, int channels)
{
    if (path.testExtension("png"))
        return savePng(path, img, w, h, channels);

    return false;
}

}

}
