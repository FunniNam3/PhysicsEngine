#include <string>
#include <stdexcept>
#include <math.h>

using namespace std;

class vector2D
{
private:
    float p[2] = {0, 0};

public:
    float *x = &(p[0]);
    float *y = &(p[1]);

    friend string to_string(vector2D in)
    {
        string o = "(";
        o += to_string(in.p[0]);
        o += ", ";
        o += to_string(in.p[1]);
        o += ")";
        return o;
    }

    vector2D(float _p1 = 0, float _p2 = 0)
    {
        p[0] = _p1;
        p[1] = _p2;
    }

    vector2D(vector2D &in)
    {
        p[0] = in.p[0];
        p[1] = in.p[1];
    }

    vector2D &operator+=(vector2D &rhs)
    {
        p[0] += rhs.p[0];
        p[1] += rhs.p[1];
        return *this;
    }

    vector2D operator+(vector2D rhs)
    {
        return vector2D(p[0] + rhs.p[0], p[1] + rhs.p[1]);
    }

    vector2D &operator*=(float &in)
    {
        p[0] *= in;
        p[1] *= in;
        return *this;
    }

    vector2D operator*(float in)
    {
        return vector2D(p[0] * in, p[1] * in);
    }

    vector2D &operator-=(vector2D &rhs)
    {
        p[0] -= rhs.p[0];
        p[1] -= rhs.p[1];
        return *this;
    }

    vector2D operator-(vector2D rhs)
    {
        return vector2D(p[0] - rhs.p[0], p[1] - rhs.p[1]);
    }

    vector2D &operator/=(float &in)
    {
        p[0] /= in;
        p[1] /= in;
        return *this;
    }

    vector2D operator/(float in)
    {
        return vector2D(p[0] / in, p[1] / in);
    }

    vector2D &operator=(const vector2D &rhs)
    {
        p[0] = rhs.p[0];
        p[1] = rhs.p[1];
        return *this;
    }

    float operator[](int i)
    {
        if (0 <= i && i <= 1)
        {
            return p[i];
        }
        throw out_of_range("Error: index does not exist");
    }

    float dot(vector2D in)
    {
        return (p[0] * in.p[0]) + (p[1] * in.p[1]);
    }

    vector2D normalize()
    {
        return (*this) / length();
    }

    float length()
    {
        return sqrt(p[0] * p[0] + p[1] * p[1]);
    }

    float getX() const
    {
        return p[0];
    }

    float getY() const
    {
        return p[1];
    }
};

class vector3D
{
private:
    float p[3] = {0, 0, 0};

public:
    float *x = &(p[0]);
    float *y = &(p[1]);
    float *z = &(p[2]);
    float *r = &(p[0]);
    float *g = &(p[1]);
    float *b = &(p[2]);

    friend string to_string(vector3D &in)
    {
        string o = "(";
        o += to_string(in.p[0]);
        o += ", ";
        o += to_string(in.p[1]);
        o += ", ";
        o += to_string(in.p[2]);
        o += ")";
        return o;
    }

    vector3D(float _p1 = 0, float _p2 = 0, float _p3 = 0)
    {
        p[0] = _p1;
        p[1] = _p2;
        p[2] = _p3;
    }

    vector3D(vector3D &in)
    {
        p[0] = in.p[0];
        p[1] = in.p[1];
        p[2] = in.p[2];
    }

    vector3D &operator+=(vector3D &rhs)
    {
        p[0] += rhs.p[0];
        p[1] += rhs.p[1];
        p[2] += rhs.p[2];
        return *this;
    }

    vector3D operator+(vector3D rhs)
    {
        return vector3D(p[0] + rhs.p[0], p[1] + rhs.p[1], p[2] + rhs.p[2]);
    }

    vector3D &operator*=(float &rhs)
    {
        p[0] *= rhs;
        p[1] *= rhs;
        p[2] *= rhs;
        return *this;
    }

    vector3D operator*(float in)
    {
        return vector3D(p[0] * in, p[1] * in, p[2] * in);
    }

    vector3D &operator-=(vector3D &rhs)
    {
        p[0] -= rhs.p[0];
        p[1] -= rhs.p[1];
        p[2] -= rhs.p[2];
        return *this;
    }

    vector3D operator-(vector3D rhs)
    {
        return vector3D(p[0] - rhs.p[0], p[1] - rhs.p[1], p[2] - rhs.p[2]);
    }

    vector3D &operator/=(float &rhs)
    {
        p[0] /= rhs;
        p[1] /= rhs;
        p[2] /= rhs;
        return *this;
    }

    vector3D operator/(float rhs)
    {
        return vector3D(p[0] / rhs, p[1] / rhs, p[2] / rhs);
    }

    vector3D &operator=(vector3D rhs)
    {
        p[0] = rhs.p[0];
        p[1] = rhs.p[1];
        p[2] = rhs.p[2];
        return *this;
    }

    float operator[](int i)
    {
        if (0 <= i && i <= 2)
        {
            return p[i];
        }
        throw out_of_range("Error: index does not exist");
    }

    float dot(vector3D &in)
    {
        return (p[0] * in.p[0]) + (p[1] * in.p[1]) + (p[2] * in.p[2]);
    }

    float length()
    {
        return sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    }

    vector3D normalize()
    {
        return (*this) / length();
    }

    float getX() const
    {
        return p[0];
    }

    float getY() const
    {
        return p[1];
    }

    float getZ() const
    {
        return p[2];
    }
};

class quaternion
{
private:
    float p[4] = {0, 0, 0, 0};

public:
    float *x = &(p[0]);
    float *y = &(p[1]);
    float *z = &(p[2]);
    float *w = &(p[3]);

    friend string to_string(quaternion in)
    {
        string o = "(";
        o += to_string(in.p[0]);
        o += ", ";
        o += to_string(in.p[1]);
        o += ", ";
        o += to_string(in.p[2]);
        o += ", ";
        o += to_string(in.p[3]);
        o += ")";
        return o;
    }

    quaternion(float _p1 = 0, float _p2 = 0, float _p3 = 0, float _p4 = 0)
    {
        p[0] = _p1;
        p[1] = _p2;
        p[2] = _p3;
        p[3] = _p4;
    }

    quaternion &operator=(quaternion &rhs)
    {
        p[0] = rhs.p[0];
        p[1] = rhs.p[1];
        p[2] = rhs.p[2];
        p[3] = rhs.p[3];
        return *this;
    }

    float operator[](int i) const
    {
        if (0 <= i && i <= 3)
        {
            return p[i];
        }
        throw out_of_range("Error: index does not exist");
    }

    float length()
    {
        return sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3]);
    }

    float getX() const
    {
        return (p[0]);
    }

    float getY() const
    {
        return (p[1]);
    }

    float getZ() const
    {
        return (p[2]);
    }

    float getW() const
    {
        return (p[3]);
    }
};