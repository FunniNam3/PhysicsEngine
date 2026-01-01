#include <stdio.h>
#include <iostream>
#include "./vectors.h"
#define WIDTH 600
#define HEIGHT 600

const int BYTES_PER_PIXEL = 3; /// red, green, & blue
const int FILE_HEADER_SIZE = 14;
const int INFO_HEADER_SIZE = 40;

void generateBitmapImage(unsigned char *image, int height, int width, char *imageFileName);
unsigned char *createBitmapFileHeader(int height, int stride);
unsigned char *createBitmapInfoHeader(int height, int width);

vector2D perp(vector2D p)
{
    return vector2D(*(p.y), -*(p.x));
};

struct Triangle
{
    vector2D a;
    vector2D b;
    vector2D c;

    Triangle(vector2D _a = vector2D(), vector2D _b = vector2D(), vector2D _c = vector2D()) : a(_a), b(_b), c(_c) {};

    bool isInside(vector2D p) // Unfinished
    {
        double det = (*(b.y) - *(c.y)) * (*(a.x) - *(c.x)) + (*(c.x) - *(b.x) * (*(a.y) - *(c.y)));

        if (abs(det) < 1e-9)
            return false;

        double alpha = (*(b.y) - *(c.y)) * (*(p.x) - *(c.x)) + (*(c.x) - *(b.x) * (*(p.y) - *(c.y))) / det;

        return false;
    }

    bool isInsideDotProduct(vector2D p)
    {
        vector2D ab = perp(b - a).normalize();
        vector2D bc = perp(c - b).normalize();
        vector2D ca = perp(a - c).normalize();

        vector2D pa = (p - a).normalize();
        vector2D pb = (p - b).normalize();
        vector2D pc = (p - c).normalize();

        float ap = ab.dot(pa);
        float bp = bc.dot(pb);
        float cp = ca.dot(pc);

        // cout << ap << " " << bp << " " << cp << endl;

        if (ap > 0 && bp > 0 && cp > 0)
        {
            return true;
        }
        else if (ap < 0 && bp < 0 && cp < 0)
        {
            return true;
        }
        return false;
    }
};

int main()
{
    Triangle tri = {vector2D(500, 500), vector2D(550, 300), vector2D(500, 200)};
    unsigned char image[HEIGHT][WIDTH][BYTES_PER_PIXEL];
    char *imageFileName = (char *)"bitmapImage.bmp";

    // bool temp = tri.isInsideDotProduct(vector2D(40, 100));

    int i, j;
    for (i = 0; i < HEIGHT; i++)
    {
        for (j = 0; j < WIDTH; j++)
        {
            bool temp = tri.isInsideDotProduct(vector2D(j, i));
            if (temp)
            {
                image[i][j][2] = (unsigned char)(0);   /// red
                image[i][j][1] = (unsigned char)(0);   /// green
                image[i][j][0] = (unsigned char)(255); /// blue
            }
            else
            {
                image[i][j][2] = (unsigned char)(0); /// red
                image[i][j][1] = (unsigned char)(0); /// green
                image[i][j][0] = (unsigned char)(0); /// blue
            }
        }
    }

    generateBitmapImage((unsigned char *)image, HEIGHT, WIDTH, imageFileName);
    printf("Image generated!!");
}

void generateBitmapImage(unsigned char *image, int height, int width, char *imageFileName)
{
    int widthInBytes = width * BYTES_PER_PIXEL;

    unsigned char padding[3] = {0, 0, 0};
    int paddingSize = (4 - (widthInBytes) % 4) % 4;

    int stride = (widthInBytes) + paddingSize;

    FILE *imageFile = fopen(imageFileName, "wb");

    unsigned char *fileHeader = createBitmapFileHeader(height, stride);
    fwrite(fileHeader, 1, FILE_HEADER_SIZE, imageFile);

    unsigned char *infoHeader = createBitmapInfoHeader(height, width);
    fwrite(infoHeader, 1, INFO_HEADER_SIZE, imageFile);

    int i;
    for (i = 0; i < height; i++)
    {
        fwrite(image + (i * widthInBytes), BYTES_PER_PIXEL, width, imageFile);
        fwrite(padding, 1, paddingSize, imageFile);
    }

    fclose(imageFile);
}

unsigned char *createBitmapFileHeader(int height, int stride)
{
    int fileSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

    static unsigned char fileHeader[] = {
        0,
        0, /// signature
        0,
        0,
        0,
        0, /// image file size in bytes
        0,
        0,
        0,
        0, /// reserved
        0,
        0,
        0,
        0, /// start of pixel array
    };

    fileHeader[0] = (unsigned char)('B');
    fileHeader[1] = (unsigned char)('M');
    fileHeader[2] = (unsigned char)(fileSize);
    fileHeader[3] = (unsigned char)(fileSize >> 8);
    fileHeader[4] = (unsigned char)(fileSize >> 16);
    fileHeader[5] = (unsigned char)(fileSize >> 24);
    fileHeader[10] = (unsigned char)(FILE_HEADER_SIZE + INFO_HEADER_SIZE);

    return fileHeader;
}

unsigned char *createBitmapInfoHeader(int height, int width)
{
    static unsigned char infoHeader[] = {
        0,
        0,
        0,
        0, /// header size
        0,
        0,
        0,
        0, /// image width
        0,
        0,
        0,
        0, /// image height
        0,
        0, /// number of color planes
        0,
        0, /// bits per pixel
        0,
        0,
        0,
        0, /// compression
        0,
        0,
        0,
        0, /// image size
        0,
        0,
        0,
        0, /// horizontal resolution
        0,
        0,
        0,
        0, /// vertical resolution
        0,
        0,
        0,
        0, /// colors in color table
        0,
        0,
        0,
        0, /// important color count
    };

    infoHeader[0] = (unsigned char)(INFO_HEADER_SIZE);
    infoHeader[4] = (unsigned char)(width);
    infoHeader[5] = (unsigned char)(width >> 8);
    infoHeader[6] = (unsigned char)(width >> 16);
    infoHeader[7] = (unsigned char)(width >> 24);
    infoHeader[8] = (unsigned char)(height);
    infoHeader[9] = (unsigned char)(height >> 8);
    infoHeader[10] = (unsigned char)(height >> 16);
    infoHeader[11] = (unsigned char)(height >> 24);
    infoHeader[12] = (unsigned char)(1);
    infoHeader[14] = (unsigned char)(BYTES_PER_PIXEL * 8);

    return infoHeader;
}
