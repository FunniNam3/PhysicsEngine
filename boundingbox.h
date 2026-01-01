#include "./vectors.h"

class boundingBox
{
    boundingBox translate();
    boundingBox rotate();
    boundingBox scale();
    boundingBox transform();
};

class boundingCircle : boundingBox
{
private:
    vector2D center;
    float r;

public:
    boundingCircle(vector2D _center = vector2D(), float _r = 0)
    {
        center = _center;
        r = _r;
    }

    boundingCircle(boundingCircle &input)
    {
        center = input.center;
        r = input.r;
    }
};

class boundingBox2D : boundingBox
{
private:
    vector2D v1;
    vector2D v2;

public:
    boundingBox2D(vector2D _v1 = vector2D(), vector2D _v2 = vector2D())
    {
        v1 = _v1;
        v2 = _v2;
    };

    float area()
    {
        vector2D temp = v1 - v2;
        return abs(temp.getX() * temp.getY());
    }
};

class boundingBox3D : boundingBox
{
private:
    vector3D v1;
    vector3D v2;

public:
    boundingBox3D(vector3D _v1 = vector3D(), vector3D _v2 = vector3D())
    {
        v1 = _v1;
        v2 = _v2;
    };

    float area()
    {
        vector3D temp = v1 - v2;
        return abs(temp.getX() * temp.getY() * temp.getZ());
    }
};