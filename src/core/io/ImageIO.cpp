#include "ImageIO.hpp"

#include "FileUtils.hpp"

#include "io/FileUtils.hpp"

#include "Debug.hpp"

#include <lodepng/lodepng.h>
#include <stbi/stb_image.h>
#include <cstring>

#if OPENEXR_AVAILABLE
#include <ImfChannelList.h>
#include <ImfOutputFile.h>
#include <ImfInputFile.h>
#include <ImfIO.h>
#include <half.h>
#endif

#if JPEG_AVAILABLE
#include <jpeglib.h>
#include <csetjmp>
#endif

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

#if OPENEXR_AVAILABLE
// OpenEXR found it necessary to write its own I/OStream abstractions,
// so we need something to bridge the gap to std streams
class ExrOStream : public Imf::OStream
{
    OutputStreamHandle _out;

public:
    ExrOStream(OutputStreamHandle out)
    : Imf::OStream("Tungsten Wrapped Output Stream"),
      _out(std::move(out))
    {
    }

    virtual void write(const char c[/*n*/], int n) override final
    {
        _out->write(c, n);
        if (!_out->good())
            throw std::runtime_error("ExrIStream::write failed");
    }

    virtual Imf::Int64 tellp() override final
    {
        return _out->tellp();
    }

    virtual void seekp(Imf::Int64 pos) override final
    {
        _out->seekp(pos, std::ios_base::beg);
    }
};

class ExrIStream : public Imf::IStream
{
    InputStreamHandle _in;

    std::unique_ptr<char[]> _data;
    uint64 _size, _offs;

public:
    ExrIStream(InputStreamHandle in)
    : Imf::IStream("Tungsten Wrapped Input Stream"),
      _in(std::move(in)),
      _offs(0)
    {
        _in->seekg(0, std::ios_base::end);
        _size = _in->tellg();
        _in->seekg(0, std::ios_base::beg);
    }

    virtual bool isMemoryMapped() const override
    {
        return false;
    }

    virtual bool read(char c[/*n*/], int n) override
    {
        _in->read(c, n);
        if (!_in->good())
            throw std::runtime_error("ExrIStream::read failed");
        _offs += n;
        return _offs < _size;
    }

    virtual char *readMemoryMapped (int /*n*/) override final
    {
        throw std::runtime_error("ExrIStream::readMemoryMapped should not be called");
    }

    virtual Imf::Int64 tellg() override final
    {
        return _offs;
    }

    virtual void seekg(Imf::Int64 pos) override final
    {
        _in->seekg(pos, std::ios_base::beg);
        _offs = pos;
        return;
    }

    virtual void clear() override final
    {
        _in->clear();
    }
};
#endif

static int stbiReadCallback(void *user, char *data, int size)
{
    std::istream &in = *static_cast<std::istream *>(user);
    in.read(data, size);
    return int(in.gcount());
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
#if OPENEXR_AVAILABLE
    if (path.testExtension("exr"))
        return true;
#endif

    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return false;
    return stbi_is_hdr_from_callbacks(&istreamCallback, in.get()) != 0;
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

#if OPENEXR_AVAILABLE
static std::unique_ptr<float[]> loadExr(const Path &path, TexelConversion request, int &w, int &h)
{
    InputStreamHandle inputStream = FileUtils::openInputStream(path);
    if (!inputStream)
        return nullptr;

    try {

    ExrIStream in(std::move(inputStream));
    Imf::InputFile file(in);
    Imath::Box2i dataWindow = file.header().dataWindow();
    w = dataWindow.max.x - dataWindow.min.x + 1;
    h = dataWindow.max.y - dataWindow.min.y + 1;
    int dx = dataWindow.min.x;
    int dy = dataWindow.min.y;

    const Imf::ChannelList &channels = file.header().channels();
    const Imf::Channel *rChannel = channels.findChannel("R");
    const Imf::Channel *gChannel = channels.findChannel("G");
    const Imf::Channel *bChannel = channels.findChannel("B");
    const Imf::Channel *aChannel = channels.findChannel("A");
    const Imf::Channel *yChannel = channels.findChannel("Y");

    Imf::FrameBuffer frameBuffer;

    bool isScalar = request != TexelConversion::REQUEST_RGB;
    bool acceptsAlpha =
            request == TexelConversion::REQUEST_ALPHA ||
            request == TexelConversion::REQUEST_AUTO;
    int targetChannels = isScalar ? 1 : 3;
    int sourceChannels = (rChannel ? 1 : 0) + (gChannel ? 1 : 0) + (bChannel ? 1 : 0);

    std::unique_ptr<float[]> texels(new float[w*h*targetChannels]);
    std::unique_ptr<float[]> img;
    size_t texelSize = sizeof(float)*targetChannels;
    char *base = reinterpret_cast<char *>(texels.get() - (dx + dy*w)*targetChannels);

    if (isScalar && (yChannel != nullptr || (acceptsAlpha && aChannel != nullptr))) {
        // The user requested a scalar texture and the file either contains an alpha channel or a luminance channel
        // The alpha channel is only allowed if it was explicitly requested or an auto conversion was requested
        // RGB -> Scalar falls through and is handled later
        const char *channelName = (acceptsAlpha && aChannel != nullptr) ? "A" : "Y";
        frameBuffer.insert(channelName, Imf::Slice(Imf::FLOAT, base, texelSize, texelSize*w));
    } else if (!isScalar && (rChannel || gChannel || bChannel)) {
        // The user wants RGB and we have (some) RGB channels
        if (rChannel) frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base + 0*sizeof(float), texelSize, texelSize*w));
        if (gChannel) frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + 1*sizeof(float), texelSize, texelSize*w));
        if (bChannel) frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2*sizeof(float), texelSize, texelSize*w));
        // If some channels are missing, replace them with black
        if (!rChannel || !gChannel || !bChannel)
            std::memset(texels.get(), 0, texelSize*w*h);
    } else if (!isScalar && yChannel) {
        // The user wants RGB and we have just luminance -> duplicate luminance across all three channels
        // This is not the best solution, but we don't want to deal with chroma subsampled images
        frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT, base, texelSize, texelSize*w));
    } else if (isScalar) {
        // The user wants a scalar texture and we have (some) RGB channels
        // We can't read directly into the destination, but have to read to a temporary
        // and do a conversion to scalar later
        img.reset(new float[sourceChannels*w*h]);
        char *imgBase = reinterpret_cast<char *>(img.get() - (dx + dy*w)*sourceChannels);

        const Imf::Channel *channels[] = {rChannel, gChannel, bChannel};
        const char *channelNames[] = {"R", "G", "B"};
        int texel = 0;
        for (int i = 0; i < 3; ++i) {
            if (channels[i]) {
                frameBuffer.insert(channelNames[i], Imf::Slice(Imf::FLOAT, imgBase + texel*sizeof(float),
                        sourceChannels*sizeof(float), sourceChannels*sizeof(float)*w));
                texel++;
            }
        }
    } else {
        return nullptr;
    }

    file.setFrameBuffer(frameBuffer);
    file.readPixels(dataWindow.min.y, dataWindow.max.y);

    if (img) {
        // We only get here if we need to make a scalar texture from an RGB input
        int texel = 0;
        int rLocation = -1, gLocation = -1, bLocation = -1;
        if (rChannel) rLocation = texel++;
        if (gChannel) gLocation = texel++;
        if (bChannel) bLocation = texel++;

        for (int i = 0; i < w*h; ++i) {
            float r = rChannel ? img[i*sourceChannels + rLocation] : 0.0f;
            float g = gChannel ? img[i*sourceChannels + gLocation] : 0.0f;
            float b = bChannel ? img[i*sourceChannels + bLocation] : 0.0f;
            texels[i] = convertToScalar(request, r, g, b, 0.0f, false);
        }
    } else if (!isScalar && yChannel) {
        // Here we need to duplicate the Y channel stored in R across G and B
        for (int i = 0; i < w*h; ++i)
            texels[i*3 + 1] = texels[i*3 + 2] = texels[i*3];
    }

    return std::move(texels);

    } catch(const std::exception &e) {
        std::cout << "OpenEXR loader failed: " << e.what() << std::endl;
        return nullptr;
    }
}
#endif

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
#if OPENEXR_AVAILABLE
    else if (path.testExtension("exr"))
        return std::move(loadExr(path, request, w, h));
#endif
    else
        return std::move(loadStbiHdr(path, request, w, h));
}

typedef std::unique_ptr<uint8[], void(*)(void *)> DeletablePixels;
static void nop(void *) {}
static inline DeletablePixels makeVoidPixels()
{
    return DeletablePixels((uint8 *)0, &nop);
}

DeletablePixels loadPng(const Path &path, int &w, int &h, int &channels)
{
    uint64 size = FileUtils::fileSize(path);
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (size == 0 || !in)
        return makeVoidPixels();

    std::unique_ptr<uint8[]> file(new uint8[size_t(size)]);
    FileUtils::streamRead(in, file.get(), size_t(size));

    uint8 *dst = nullptr;
    uint32 uw, uh;
    lodepng_decode_memory(&dst, &uw, &uh, file.get(), size_t(size), LCT_RGBA, 8);
    if (!dst)
        return makeVoidPixels();

    w = uw;
    h = uh;
    channels = 4;

    return DeletablePixels(dst, free);
}

#if JPEG_AVAILABLE
DeletablePixels loadJpg(const Path &path, int &w, int &h, int &channels)
{
    struct jpeg_decompress_struct cinfo;
    struct CustomJerr : jpeg_error_mgr {
        std::string fileName;
        std::jmp_buf env;
    } jerr;
    jerr.fileName = path.fileName().asString();
    if (setjmp(jerr.env))
        return makeVoidPixels();

    cinfo.err = jpeg_std_error(&jerr);
    jerr.output_message = [](j_common_ptr cinfo) {
        char buffer[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message) (cinfo, buffer);
        std::cout << tfm::format("Jpg decoding issue for file '%s': %s\n",
                static_cast<CustomJerr *>(cinfo->err)->fileName, buffer);
        std::cout.flush();
    };
    jerr.error_exit = [](j_common_ptr cinfo) {
        (*cinfo->err->output_message)(cinfo);
        std::longjmp(static_cast<CustomJerr *>(cinfo->err)->env, 1);
    };

    jpeg_create_decompress(&cinfo);

    uint64 fileSize = FileUtils::fileSize(path);
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return makeVoidPixels();

    // Could use more sophisticated JPEG input handler here, but they are a pain to implement
    // For now, just read the file straight to memory
    std::unique_ptr<uint8[]> fileBuf(new uint8[fileSize]);
    FileUtils::streamRead(in, fileBuf.get(), fileSize);
    jpeg_mem_src(&cinfo, fileBuf.get(), fileSize);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    w = cinfo.output_width;
    h = cinfo.output_height;
    channels = 4;

    cinfo.out_color_space = JCS_RGB;

    DeletablePixels result(new uint8[cinfo.output_width*cinfo.output_height*4], [](void *p) {
        delete[] reinterpret_cast<uint8 *>(p);
    });

    std::unique_ptr<unsigned char *[]> scanlines(new unsigned char *[cinfo.output_height]);
    for (unsigned i = 0; i < cinfo.output_height; ++i)
        scanlines[i] = &result[i*cinfo.output_width*4];
    while (cinfo.output_scanline < cinfo.output_height)
        jpeg_read_scanlines(&cinfo, &scanlines[cinfo.output_scanline], cinfo.output_height - cinfo.output_scanline);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    for (int y = 0; y < h; ++y) {
        for (int x = w - 1; x >= 0; --x) {
            result[y*w*4 + x*4 + 3] = 0xFF;
            result[y*w*4 + x*4 + 2] = result[y*w*4 + x*3 + 2];
            result[y*w*4 + x*4 + 1] = result[y*w*4 + x*3 + 1];
            result[y*w*4 + x*4 + 0] = result[y*w*4 + x*3 + 0];
        }
    }

    return std::move(result);
}
#endif

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
#if JPEG_AVAILABLE
    else if (path.testExtension("jpg") || path.testExtension("jpeg"))
        img = loadJpg(path, w, h, channels);
#endif
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
    for (int y = 0; y < h; ++y)
        out->write(reinterpret_cast<const char *>(img + (h - y - 1)*w*channels), w*channels*sizeof(float));

    return true;
}

#if OPENEXR_AVAILABLE
bool saveExr(const Path &path, const float *img, int w, int h, int channels)
{
    if (channels <= 0 || channels > 4)
        return false;

    OutputStreamHandle outputStream = FileUtils::openOutputStream(path);
    if (!outputStream)
        return false;

    try {

    Imf::Header header(w, h, 1.0f, Imath::V2f(0, 0), 1.0f, Imf::INCREASING_Y, Imf::PIZ_COMPRESSION);
    Imf::FrameBuffer frameBuffer;

    std::unique_ptr<half[]> data(new half[w*h*channels]);
    for (int i = 0; i < w*h*channels; ++i)
        data[i] = half(img[i]);

    const char *channelNames[] = {"R", "G", "B", "A"};
    for (int i = 0; i < channels; ++i) {
        const char *channelName = (channels == 1) ? "Y" : channelNames[i];
        header.channels().insert(channelName, Imf::Channel(Imf::HALF));
        frameBuffer.insert(channelName, Imf::Slice(Imf::HALF, reinterpret_cast<char *>(data.get() + i),
                sizeof(half)*channels, sizeof(half)*channels*w));
    }

    ExrOStream out(std::move(outputStream));
    Imf::OutputFile file(out, header);
    file.setFrameBuffer(frameBuffer);
    file.writePixels(h);

    return true;

    } catch(const std::exception &e) {
        std::cout << "OpenEXR writer failed: " << e.what() << std::endl;
        return false;
    }
}
#endif

bool savePng(const Path &path, const uint8 *img, int w, int h, int channels)
{
    if (channels <= 0 || channels > 4)
        return false;
    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out) {
        DBG("Unable to save PNG at path '%s'", path.absolute());
        return false;
    }

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
#if OPENEXR_AVAILABLE
    else if (path.testExtension("exr"))
        return saveExr(path, img, w, h, channels);
#endif

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
