#include "vec.h"

inline
double
v2_modulus(v2 v)
{
        return sqrt(v.x * v.x + v.y * v.y);
}

inline
double
v2_distance(v2 v, v2 w)
{
        return v2_modulus((v2) {v.x - w.x, v.y - w.y});
}

inline
v2
v2_mid(v2 v, v2 w)
{
        return (v2) {(v.x + w.x) / 2., (v.y + w.y) / 2.};
}

inline
double
v2_angle(v2 v, v2 w)
{
        return atan2(
                w.y * v.x - w.x * v.y,
                w.x * v.x + w.y * v.y
        );
}

inline
v2
v2_rotate(v2 v, float rad)
{
        v2 w;

        /* Multiply by non-standard rotation matrix */
        w.x =   v.x  * cosf(rad) + v.y * sinf(rad);
        w.y = (-v.x) * sinf(rad) + v.y * cosf(rad);

        return w;
}
