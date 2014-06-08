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

void computeZigZag(const int N, int *zigZag)
{
    int iter = 0;
    for (int d = 0; d < N; ++d) {
        for (int t = 0; t <= d; ++t) {
            if (d & 1)
                zigZag[(d - t) + t*N] = iter++;
            else
                zigZag[(d - t)*N + t] = iter++;
        }
    }
    for (int d = 1; d < N; ++d) {
        for (int t = 0; t < N - d; ++t) {
            if (d & 1)
                zigZag[(d + t) + (N - 1 - t)*N] = iter++;
            else
                zigZag[(d + t)*N + (N - 1 - t)] = iter++;
        }
    }
}

void computeQuantizationMatrix(const int N, int *qMatrix)
{
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            float fx = x*8.0f/N;
            float fy = y*8.0f/N;
            int x0 = int(fx), y0 = int(fy);
            int x1 = min(x0 + 1, 7), y1 = min(y0 + 1, 7);
            float dx = fx - x0;
            float dy = fy - y0;

            qMatrix[y*N + x] = int(
                (jpegMatrix[y0][x0]*(1.0f - dx) + jpegMatrix[y1][x0]*dx)*(1.0f - dy) +
                (jpegMatrix[y0][x1]*(1.0f - dx) + jpegMatrix[y1][x1]*dx)*dy
            );
            //qMatrix[y*N + x] = 16;
        }
    }
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
                dst[y*N + x] = clamp(128 + (int(srcBlock[y*N + x]) - int(prediction[y*N + x]))/2, 0, 255);
    } else {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                dst[y*N + x] = srcBlock[y*N + x];
    }

    return type;
}

void decodeDiff(uint8 *src, const int bx, const int by, const int N, const int stride, const int type)
{
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
            src[(by + y)*stride + bx + x] = clamp((int(src[(by + y)*stride + bx + x]) - 128)*2 + prediction[y*N + x], 0, 255);
}

void rgbToYCbCr(uint8 &Y, uint8 &Cb, uint8 &Cr, const uint8 r, const uint8 g, const uint8 b)
{
    Y  = clamp(int(std::round(      r*0.299f    + g*0.587f    + b*0.114f   )), 0, 255);
    Cb = clamp(int(std::round(128 - r*0.168736f - g*0.331264f + b*0.5f     )), 0, 255);
    Cr = clamp(int(std::round(128 + r*0.5f      - g*0.418688f - b*0.081312f)), 0, 255);
}

void yCbCrToRgb(uint8 &r, uint8 &g, uint8 &b, const uint8 Y, const uint8 Cb, const uint8 Cr)
{
    r = clamp(int(std::round(Y +                              1.402f*(int(Cr) - 128))), 0, 255);
    g = clamp(int(std::round(Y - 0.34414f*(int(Cb) - 128) - 0.71414f*(int(Cr) - 128))), 0, 255);
    b = clamp(int(std::round(Y +   1.772f*(int(Cb) - 128)                           )), 0, 255);
}

void blockEncodeImage(uint8 *predictorDst, uint8 *blockDst, uint8 *src, const int *qMatrix,
        const int *zigZag, const int w, const int h, const int componentStride, const int N)
{
    float srcBlock[N][N], tmpBlock[N][N], dstBlock[N][N], dctTable[N][N], idctTable[N][N];
    computeDctTables(&dctTable[0][0], &idctTable[0][0], N);
    uint8 tmp[N*N];

    for (int by = 0, blockIndex = 0; by < h; by += N) {
        for (int bx = 0; bx < w; bx += N, ++blockIndex) {
            int predictor = encodeBestDiff(tmp, src, bx, by, N, w);
            if (blockIndex > 0) {
                int byte = blockIndex/4;
                int bit = (blockIndex % 4)*2;
                predictorDst[byte] |= predictor << bit;
            }

            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                    srcBlock[y][x] = int(tmp[y*N + x]) - 128;

            dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &dctTable[0][0], N);

            for (int y = 0; y < N; ++y) {
                for (int x = 0; x < N; ++x) {
                    int value = clamp(int(std::round(dstBlock[y][x]/qMatrix[y*N + x])), -128, 127);
                    blockDst[zigZag[y*N + x]*componentStride + blockIndex] = value;
                    dstBlock[y][x] = value*qMatrix[y*N + x];
                }
            }

            dct2D(&srcBlock[0][0], &tmpBlock[0][0], &dstBlock[0][0], &idctTable[0][0], N);

            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                    src[(by + y)*w + (bx + x)] = clamp(int(std::round(srcBlock[y][x] + 128)), 0, 255);
            if (blockIndex > 0)
                decodeDiff(src, bx, by, N, w, predictor);
        }
    }
}

void blockDecodeImage(uint8 *dst, const uint8 *predictorSrc, const uint8 *blockSrc, const int *qMatrix,
        const int *zigZag,  const int w, const int h, const int componentStride, const int N)
{
    float srcBlock[N][N], tmpBlock[N][N], dstBlock[N][N], dctTable[N][N], idctTable[N][N];
    computeDctTables(&dctTable[0][0], &idctTable[0][0], N);

    for (int by = 0, blockIndex = 0; by < h; by += N) {
        for (int bx = 0; bx < w; bx += N, ++blockIndex) {
            for (int y = 0; y < N; ++y) {
                for (int x = 0; x < N; ++x) {
                    uint8 srcByte = blockSrc[zigZag[y*N + x]*componentStride + blockIndex];
                    srcBlock[y][x] = int(int8(srcByte))*qMatrix[y*N + x];
                }
            }

            dct2D(&dstBlock[0][0], &tmpBlock[0][0], &srcBlock[0][0], &idctTable[0][0], N);

            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                    dst[(by + y)*w + (bx + x)] = clamp(int(std::round(dstBlock[y][x] + 128)), 0, 255);

            int predictorByte = blockIndex/4;
            int predictorBit = (blockIndex % 4)*2;
            int predictor = (predictorSrc[predictorByte] >> predictorBit) & 3;
            if (blockIndex > 0)
                decodeDiff(dst, bx, by, N, w, predictor);
        }
    }
}

uint8 *encodeGrayscaleImage(uint8 *src, const int w, const int h, const int N, int &bufSize)
{
    int blocksX = w/N;
    int blocksY = h/N;
    int componentStride = blocksX*blocksY;

    int qMatrix[N*N], zigZag[N*N];
    computeQuantizationMatrix(N, qMatrix);
    computeZigZag(N, zigZag);

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes + srcBytes;

    uint8 *srcBuf = new uint8[totalSize];
    uint8 *dstBuf = new uint8[totalSize];
    std::memset(srcBuf, 0, totalSize);

    blockEncodeImage(srcBuf, srcBuf + predictorBytes, src, qMatrix, zigZag, w, h, componentStride, N);

    uLongf compressedSize = totalSize;
    compress2(dstBuf, &compressedSize, srcBuf, totalSize, 9);

    std::cout << "Compressed size: " << compressedSize <<
            " Total compression ratio: " << (compressedSize*100.0f)/(w*h) << "%" << std::endl;

    delete[] srcBuf;

    bufSize = compressedSize;
    return dstBuf;
}

void decodeGrayscaleImage(uint8 *dst, const uint8 *src, const int bufSize, const int w, const int h, const int N)
{
    int blocksX = w/N;
    int blocksY = h/N;
    int componentStride = blocksX*blocksY;

    int qMatrix[N*N], zigZag[N*N];
    computeQuantizationMatrix(N, qMatrix);
    computeZigZag(N, zigZag);

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes + srcBytes;
    uint8 *dstBuf = new uint8[totalSize];

    uLongf uncomprSize = totalSize;
    uncompress(dstBuf, &uncomprSize, src, bufSize);

    blockDecodeImage(dst, dstBuf, dstBuf + predictorBytes, qMatrix, zigZag, w, h, componentStride, N);

    delete[] dstBuf;
}

uint8 *encodeColorImage(uint8 *src, int w, int h, const int N, int &bufSize)
{
    uint8 * yImg = new uint8[w*h];
    uint8 *cbImg = new uint8[w*h];
    uint8 *crImg = new uint8[w*h];

    for (int i = 0; i < w*h; ++i)
        rgbToYCbCr(yImg[i], cbImg[i], crImg[i], src[i*3], src[i*3 + 1], src[i*3 + 2]);

    int blocksX = w/N;
    int blocksY = h/N;
    int componentStride = blocksX*blocksY;

    int qMatrix[N*N], zigZag[N*N];
    computeQuantizationMatrix(N, qMatrix);
    computeZigZag(N, zigZag);

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes*3 + srcBytes*3;

    uint8 *srcBuf = new uint8[totalSize];
    uint8 *dstBuf = new uint8[totalSize];
    std::memset(srcBuf, 0, totalSize);

    blockEncodeImage(srcBuf + predictorBytes*0, srcBuf + predictorBytes*3 + srcBytes*0,  yImg, qMatrix, zigZag, w, h, componentStride, N);
    blockEncodeImage(srcBuf + predictorBytes*1, srcBuf + predictorBytes*3 + srcBytes*1, cbImg, qMatrix, zigZag, w, h, componentStride, N);
    blockEncodeImage(srcBuf + predictorBytes*2, srcBuf + predictorBytes*3 + srcBytes*2, crImg, qMatrix, zigZag, w, h, componentStride, N);

    uLongf compressedSize = totalSize;
    compress2(dstBuf, &compressedSize, srcBuf, totalSize, 9);

    std::cout << "Compressed size: " << compressedSize <<
            " Total compression ratio: " << (compressedSize*100.0f)/(w*h*3) << "%" << std::endl;

    delete[] srcBuf;
    delete[]  yImg;
    delete[] cbImg;
    delete[] crImg;

    bufSize = compressedSize;
    return dstBuf;
}

void decodeColorImage(uint8 *dst, const uint8 *src, const int bufSize, const int w, const int h, const int N)
{
    int blocksX = w/N;
    int blocksY = h/N;
    int componentStride = blocksX*blocksY;

    int qMatrix[N*N], zigZag[N*N];
    computeQuantizationMatrix(N, qMatrix);
    computeZigZag(N, zigZag);

    int predictorBytes = (blocksX*blocksY + 3)/4;
    int srcBytes = w*h;
    int totalSize = predictorBytes*3 + srcBytes*3;
    uint8 *dstBuf = new uint8[totalSize];

    uLongf uncomprSize = totalSize;
    uncompress(dstBuf, &uncomprSize, src, bufSize);

    uint8 * yImg = new uint8[w*h];
    uint8 *cbImg = new uint8[w*h];
    uint8 *crImg = new uint8[w*h];

    blockDecodeImage( yImg, dstBuf + predictorBytes*0, dstBuf + predictorBytes*3 + srcBytes*0, qMatrix, zigZag, w, h, componentStride, N);
    blockDecodeImage(cbImg, dstBuf + predictorBytes*1, dstBuf + predictorBytes*3 + srcBytes*1, qMatrix, zigZag, w, h, componentStride, N);
    blockDecodeImage(crImg, dstBuf + predictorBytes*2, dstBuf + predictorBytes*3 + srcBytes*2, qMatrix, zigZag, w, h, componentStride, N);

    for (int i = 0; i < w*h; ++i)
        yCbCrToRgb(dst[i*3], dst[i*3 + 1], dst[i*3 + 2], yImg[i], cbImg[i], crImg[i]);

    delete[] dstBuf;
    delete[]  yImg;
    delete[] cbImg;
    delete[] crImg;
}

uint8 *loadPaddedImage(int &w, int &h, const std::string &path, const int comp, const int blockSize)
{
    int tmp;
    uint8 *img = stbi_load(path.c_str(), &w, &h, &tmp, comp);

    if (img) {
        if (h % blockSize || w % blockSize) {
            int newH = (h & ~(blockSize - 1)) + blockSize;
            int newW = (w & ~(blockSize - 1)) + blockSize;

            uint8 *newImg = new uint8[newW*newH*comp];
            std::memset(newImg, 0, newW*newH*comp);
            for (int y = 0; y < newH; ++y)
                for (int x = 0; x < newW; ++x)
                    for (int i = 0; i < comp; ++i)
                        newImg[(x + y*newW)*comp + i] = img[(min(x, w - 1) + min(y, h - 1)*w)*comp + i];
            img = newImg;
            w = newW;
            h = newH;
        }
    }

    return img;
}

void saveGrayscale(const std::string &path, uint8 *src, int w, int h)
{
    uint8 *tmp = new uint8[w*h*3];
    for (int i = 0; i < w*h; ++i)
        tmp[i*3 + 0] = tmp[i*3 + 1] = tmp[i*3 + 2] = src[i];

    lodepng_encode24_file(path.c_str(), tmp, w, h);

    delete[] tmp;
}

void testColorImage(const std::string &path)
{
    const int N = 16;
    int w, h;
    uint8 *img = loadPaddedImage(w, h, path, 3, N);

    if (img) {
        std::string encodedImg = FileUtils::stripExt(path) + "-encoded.png";
        std::string decodedImg = FileUtils::stripExt(path) + "-decoded.png";

        int bufSize;
        uint8 *compressed = encodeColorImage(img, w, h, N, bufSize);
        decodeColorImage(img, compressed, bufSize, w, h, N);
        lodepng_encode24_file(decodedImg.c_str(), img, w, h);

        delete[] compressed;
    }
}

void testGrayscaleImage(const std::string &path)
{
    const int N = 16;

    int w, h;
    uint8 *img = loadPaddedImage(w, h, path, 1, N);

    if (img) {
        std::string encodedImg = FileUtils::stripExt(path) + "-encoded.png";
        std::string decodedImg = FileUtils::stripExt(path) + "-decoded.png";

        int bufSize;
        uint8 *compressed = encodeGrayscaleImage(img, w, h, N, bufSize);
        decodeGrayscaleImage(img, compressed, bufSize, w, h, N);
        saveGrayscale(decodedImg, img, w, h);

        delete[] compressed;
    }
}

int main()
{
//  dctTest();
//  idctTest();
//  transformTest();
//  testGrayscaleImage("C:/Users/Tuna brain/Desktop/TestImages/hdr.png");
    testColorImage("C:/Users/Tuna brain/Desktop/TestImages/rgb/spider_web.png");
    return 0;
}
