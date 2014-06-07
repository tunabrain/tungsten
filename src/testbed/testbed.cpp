#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "io/FileUtils.hpp"
#include "IntTypes.hpp"

#include <extern/lodepng/lodepng.h>
#include <extern/stbi/stb_image.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <zlib.h>

using namespace Tungsten;

void dctTest()
{
    constexpr int N = 8;

    uint8 result[N*N][N*N][3];

    for (int by = 0; by < N; ++by) {
        for (int bx = 0; bx < N; ++bx) {
            for (int y = 0; y < N; ++y) {
                for (int x = 0; x < N; ++x) {
                    float Xk = std::cos(PI/N*(x + 0.5f)*bx);
                    float Yk = std::cos(PI/N*(y + 0.5f)*by);
                    uint8 r = int((Xk*Yk*0.5f + 0.5f)*255.0f);
                    uint8 *c = &result[by*N + y][bx*N + x][0];
                    c[0] = c[1] = c[2] = r;
                }
            }
        }
    }

    lodepng_encode24_file("DCT.png", &result[0][0][0], N*N, N*N);
}

void idctTest()
{
    constexpr int N = 8;

    uint8 result[N*N][N*N][3];

    for (int by = 0; by < N; ++by) {
        for (int bx = 0; bx < N; ++bx) {
            for (int y = 0; y < N; ++y) {
                for (int x = 0; x < N; ++x) {
                    float Xk = std::cos(PI/N*(bx + 0.5f)*x);
                    float Yk = std::cos(PI/N*(by + 0.5f)*y);
                    uint8 r = int((Xk*Yk*0.5f + 0.5f)*255.0f);
                    uint8 *c = &result[by*N + y][bx*N + x][0];
                    c[0] = c[1] = c[2] = r;
                }
            }
        }
    }

    lodepng_encode24_file("DCT-Inv.png", &result[0][0][0], N*N, N*N);
}

void computeDctTables(float *dct, float *idct, const int N)
{
    for (int bx = 0; bx < N; ++bx) {
        for (int x = 0; x < N; ++x) {
             dct[bx + x*N] = bx == 0 ? 1.0f/std::sqrt(2.0f) : std::cos(PI/N*(x + 0.5f)*bx);
            idct[bx + x*N] =  x == 0 ? 1.0f/std::sqrt(2.0f) : std::cos(PI/N*(bx + 0.5f)*x);
        }
    }
}

void dctKernel1D(float *acc, const float *src, const float *dctTable, const int k, const int N)
{
    for (int i = 0; i < N; ++i)
        acc[i] += dctTable[k*N + i]*src[k];
}

void dct1D(float *acc, const float *src, const float *dctTable, const int N)
{
    for (int k = 0; k < N; ++k)
        dctKernel1D(acc, src, dctTable, k, N);
}

void dct2D(float *dst, float *tmp, const float *src, const float *dctTable, const int N)
{
    std::memset(tmp, 0, N*N*sizeof(float));
    std::memset(dst, 0, N*N*sizeof(float));

    for (int r = 0; r < N; ++r)
        for (int k = 0; k < N; ++k)
            for (int i = 0; i < N; ++i)
                tmp[i*N + r] += dctTable[k*N + i]*src[r*N + k];
    for (int r = 0; r < N; ++r)
        for (int k = 0; k < N; ++k)
            for (int i = 0; i < N; ++i)
                dst[i*N + r] += dctTable[k*N + i]*tmp[r*N + k];

    const float scale = 2.0f/N;
    for (int i = 0; i < N*N; ++i)
        dst[i] *= scale;
}

static const int jpegMatrix[8][8] = {
    {16, 11, 10, 16,  24,  40,  51,  61},
    {12, 12, 14, 19,  26,  58,  60,  55},
    {14, 13, 16, 24,  40,  57,  69,  56},
    {14, 17, 22, 29,  51,  87,  80,  62},
    {18, 22, 37, 56,  68, 109, 103,  77},
    {24, 35, 55, 64,  81, 104, 113,  92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103,  99},
};
static int qMatrix[8][8];

void computeQMatrix()
{
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            //qMatrix[y][x] = 16;
            qMatrix[y][x] = jpegMatrix[y][x];
        }
    }
}

static const int zigZag[8][8] = {
    { 0,  1,  5,  6, 14, 15, 27, 28},
    { 2,  4,  7, 13, 16, 26, 29, 42},
    { 3,  8, 12, 17, 25, 30, 41, 43},
    { 9, 11, 18, 24, 31, 40, 44, 53},
    {10, 19, 23, 32, 39, 45, 52, 54},
    {20, 22, 33, 38, 46, 51, 55, 60},
    {21, 34, 37, 47, 50, 56, 59, 61},
    {35, 36, 48, 49, 57, 58, 62, 63},
};

void transformTest()
{
    float dst[8][8], result[8][8], tmp[8][8];
    float dctTable[8][8], idctTable[8][8];

    float src[8][8] = {
        {52, 55, 61,  66,  70,  61, 64, 73},
        {63, 59, 55,  90, 109,  85, 69, 72},
        {62, 59, 68, 113, 144, 104, 66, 73},
        {63, 58, 71, 122, 154, 106, 70, 69},
        {67, 61, 68, 104, 126,  88, 68, 70},
        {79, 65, 60,  70,  77,  68, 58, 75},
        {85, 71, 64,  59,  55,  61, 65, 83},
        {87, 79, 69,  68,  65,  76, 78, 94},
    };

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            src[y][x] -= 128;

    computeDctTables(&dctTable[0][0], &idctTable[0][0], 8);
    dct2D(&dst[0][0], &tmp[0][0], &src[0][0], &dctTable[0][0], 8);

    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            dst[y][x] = std::round(dst[y][x]/qMatrix[y][x])*qMatrix[y][x];

    dct2D(&result[0][0], &tmp[0][0], &dst[0][0], &idctTable[0][0], 8);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            std::cout << std::round(result[y][x] + 128) << " ";
            //std::cout << dst[y][x] << " ";
        }
        std::cout << std::endl;
    }
}

#define PREDICT_H  0
#define PREDICT_V  1
#define PREDICT_DC 2
#define PREDICT_TM 3

void blockPrediction(uint8 *dst, const uint8 *left, const uint8 *top, const uint8 c, const int N, const int type)
{
    switch (type) {
    case PREDICT_H:
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[y*N + x] = left[y];
        break;
    case PREDICT_V:
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[y*N + x] = top[x];
        break;
    case PREDICT_DC: {
        int dc = 0;
        for (int i = 0; i < N; ++i)
            dc += int(left[i]) + int(top[i]);
        dc /= 2*N;
        for (int i = 0; i < N*N; ++i)
            dst[i] = dc;
        break;
    }
    case PREDICT_TM:
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[y*N + x] = clamp(int(left[y]) + int(top[x]) - int(c), 0, 255);
        break;
    }
}

int blockError(const uint8 *a, const uint8 *b, const int N)
{
    int error = 0;
    for (int i = 0; i < N*N; ++i)
        error += std::abs(int(a[i]) - int(b[i]));
    return error;
}

void checkPrediction(uint8 *prediction, const uint8 *src, const uint8 *left,
        const uint8 *top, const uint8 c, const int N, const int type, int &bestError, int &bestType)
{
    blockPrediction(prediction, left, top, c, N, type);
    int predError = blockError(src, prediction, N);
    if (predError < bestError) {
        bestType = type;
        bestError = predError;
    }
}

int encodeBestDiff(uint8 *dst, uint8 *src, const int bx, const int by, const int N, const int stride)
{
    uint8 srcBlock[N*N];
    uint8 prediction[N*N];
    uint8 left[N], top[N];
    uint8 c = 0;

    int type = -1;
    int error = N*N*256;

    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            srcBlock[y*N + x] = src[(by + y)*stride + bx + x];

    if (bx) {
        for (int y = 0; y < N; ++y)
            left[y] = src[(by + y)*stride + bx - 1];
        checkPrediction(prediction, srcBlock, left, top, c, N, PREDICT_H, error, type);
    }
    if (by) {
        for (int x = 0; x < N; ++x)
            top[x] = src[(by - 1)*stride + bx + x];
        checkPrediction(prediction, srcBlock, left, top, c, N, PREDICT_V, error, type);
    }
    if (bx && by) {
        c = src[(by - 1)*stride + bx - 1];

        checkPrediction(prediction, srcBlock, left, top, c, N, PREDICT_DC, error, type);
        checkPrediction(prediction, srcBlock, left, top, c, N, PREDICT_TM, error, type);
    }

    if (type != -1) {
        blockPrediction(prediction, left, top, c, N, type);
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[(by + y)*stride + bx + x] = clamp(128 + int(srcBlock[y*N + x]) - int(prediction[y*N + x]), 0, 255);
                //dst[(by + y)*stride + bx + x] = std::abs(int(srcBlock[y*N + x]) - int(prediction[y*N + x]));
    } else {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[(by + y)*stride + bx + x] = srcBlock[y*N + x];
    }

    return type;
}

void decodeDiff(uint8 *src, const int bx, const int by, const int N, const int stride, const int type)
{
    if (type == -1)
        return;

    uint8 left[N], top[N];
    uint8 c = 0;

    if (bx)
        for (int y = 0; y < N; ++y)
            left[y] = src[(by + y)*stride + bx - 1];
    if (by)
        for (int x = 0; x < N; ++x)
            top[x] = src[(by - 1)*stride + bx + x];
    if (bx && by)
        c = src[(by - 1)*stride + bx - 1];

    uint8 prediction[N*N];
    blockPrediction(prediction, left, top, c, N, type);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            src[(by + y)*stride + bx + x] = clamp(int(src[(by + y)*stride + bx + x]) + int(prediction[y*N + x]) - 128, 0, 255);
}

uint8 *encodeImage(uint8 *src, int w, int h, int &bufSize)
{
    float srcBlock[8][8];
    float tmpBlock[8][8], dstBlock[8][8];
    float dctTable[8][8], idctTable[8][8];
    computeDctTables(&dctTable[0][0], &idctTable[0][0], 8);

    int blocksX = (w + 7)/8;
    int blocksY = (h + 7)/8;
    int componentStride = (w*h)/64;

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes + srcBytes;
    uint8 *srcBuf = new uint8[totalSize];
    uint8 *dstBuf = new uint8[totalSize];
    std::memset(srcBuf, 0, totalSize);

    uint8 *tmp = new uint8[w*h];

    int *predictors = new int[blocksX*blocksY];

    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            int predictor = encodeBestDiff(tmp, src, bx, by, 8, w);
            predictors[bx/8 + (by/8)*blocksX] = predictor;

            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    srcBlock[y][x] = int(tmp[(by + y)*w + (bx + x)]) - 128;

            dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &dctTable[0][0], 8);

            int componentOffset = bx/8 + (by/8)*blocksX;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    int value = clamp(int(std::round(dstBlock[y][x]/qMatrix[y][x])), -128, 127);
                    srcBuf[zigZag[y][x]*componentStride + componentOffset + predictorBytes] = value;
                    dstBlock[y][x] = value*qMatrix[y][x];
                }
            }

            dct2D(&srcBlock[0][0], &tmpBlock[0][0], &dstBlock[0][0], &idctTable[0][0], 8);

            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    src[(by + y)*w + (bx + x)] = clamp(int(std::round(srcBlock[y][x] + 128)), 0, 255);
                }
            }
            if (bx + by > 0)
                decodeDiff(src, bx, by, 8, w, predictor);
        }
    }

    for (int i = 1; i < blocksX*blocksY; ++i) {
        int byte = i/4;
        int bit = (i % 4)*2;
        srcBuf[byte] |= predictors[i] << bit;
    }

    uLongf compressedSize = totalSize;
    std::cout << compress2(dstBuf, &compressedSize, srcBuf, totalSize, 9) << std::endl;

    std::cout << "Compressed size: " << compressedSize <<
            " Total compression ratio: " << (compressedSize*100.0f)/(w*h) << "%" << std::endl;

    delete[] tmp;
    delete[] predictors;
    delete[] srcBuf;

    bufSize = compressedSize;
    return dstBuf;
}

void decodeImage(uint8 *dst, const uint8 *src, const int bufSize, const int w, const int h)
{
    float srcBlock[8][8];
    float tmpBlock[8][8], dstBlock[8][8];
    float dctTable[8][8], idctTable[8][8];
    computeDctTables(&dctTable[0][0], &idctTable[0][0], 8);

    int blocksX = (w + 7)/8;
    int blocksY = (h + 7)/8;
    int componentStride = (w*h)/64;

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes + srcBytes;
    uint8 *dstBuf = new uint8[totalSize];

    uLongf uncomprSize = totalSize;
    std::cout << uncompress(dstBuf, &uncomprSize, src, bufSize) << std::endl;

    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            int componentOffset = bx/8 + (by/8)*blocksX;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    uint8 srcByte = dstBuf[zigZag[y][x]*componentStride + componentOffset + predictorBytes];
                    srcBlock[y][x] = int(int8(srcByte))*qMatrix[y][x];
                }
            }

            dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &idctTable[0][0], 8);

            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    dst[(by + y)*w + (bx + x)] = clamp(int(std::round(dstBlock[y][x] + 128)), 0, 255);

            int blockIndex = bx/8 + (by/8)*blocksX;
            int predictorByte = blockIndex/4;
            int predictorBit = (blockIndex % 4)*2;
            int predictor = (dstBuf[predictorByte] >> predictorBit) & 3;
            if (blockIndex == 0)
                predictor = -1;

            decodeDiff(dst, bx, by, 8, w, predictor);
        }
    }

    delete[] dstBuf;
}

void handleImage(uint8 *dst, uint8 *src, int w, int h, bool inverse)
{
    float srcBlock[8][8];
    float tmpBlock[8][8], dstBlock[8][8];

    float dctTable[8][8], idctTable[8][8];
    computeDctTables(&dctTable[0][0], &idctTable[0][0], 8);

    int predictors[4] = {0};

    for (int by = 0; by < h; by += 8)
        for (int bx = 0; bx < w; bx += 8)
            predictors[encodeBestDiff(dst, src, bx, by, 8, w)]++;
    std::cout
        << "PREDICT_H: "  << predictors[0] << std::endl
        << "PREDICT_V: "  << predictors[1] << std::endl
        << "PREDICT_DC: " << predictors[2] << std::endl
        << "PREDICT_TM: " << predictors[3] << std::endl;
    return;

    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    srcBlock[y][x] = int(src[(by + y)*w + (bx + x)]) - 128;

            if (inverse) {
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        srcBlock[y][x] = srcBlock[y][x]*qMatrix[y][x];
                dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &idctTable[0][0], 8);
            } else {
                dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &dctTable[0][0], 8);

                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        dstBlock[y][x] = std::round(dstBlock[y][x]/qMatrix[y][x]);
            }

            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    src[(by + y)*w + (bx + x)] = clamp(int(std::round(dstBlock[y][x] + 128)), 0, 255);
        }
    }
}

void saveGrayscale(const std::string &path, uint8 *src, int w, int h)
{
    uint8 *tmp = new uint8[w*h*3];
    for (int i = 0; i < w*h; ++i)
        tmp[i*3 + 0] = tmp[i*3 + 1] = tmp[i*3 + 2] = src[i];

    lodepng_encode24_file(path.c_str(), tmp, w, h);

    delete[] tmp;
}

void testGrayscaleImage(const std::string &path)
{
    int w, h, comp;
    uint8 *img = stbi_load(path.c_str(), &w, &h, &comp, 1);

    if (img) {
        if (h % 8 || w % 8) {
            std::cout << "Warning: image size " << w << "x" << h << " not divisible by 8" << std::endl;

            int newH = (h & ~7) + 8;
            int newW = (w & ~7) + 8;

            uint8 *newImg = new uint8[newW*newH];
            std::memset(newImg, 0, newW*newH);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    newImg[x + y*newW] = img[x + y*w];
            img = newImg;
            w = newW;
            h = newH;
        }

        std::string encodedImg = FileUtils::stripExt(path) + "-encoded.png";
        std::string decodedImg = FileUtils::stripExt(path) + "-decoded.png";

        int bufSize;
        uint8 *compressed = encodeImage(img, w, h, bufSize);
        decodeImage(img, compressed, bufSize, w, h);
        saveGrayscale(decodedImg, img, w, h);
    }
}

int main()
{
//  dctTest();
//  idctTest();
//  transformTest();
    computeQMatrix();
    testGrayscaleImage("C:/Users/Tuna brain/Desktop/TestImages/fireworks.png");
    return 0;
}
