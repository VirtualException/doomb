#ifndef _VEC_H
#define _VEC_H

#include <math.h>

typedef struct _v2 {
        double x;
        double y;
} v2;

double  v2_modulus(v2 v);
double  v2_distance(v2 v, v2 w);
v2      v2_mid(v2 v, v2 w);
double  v2_angle(v2 v, v2 w);
v2      v2_rotate(v2 v, float rad);

#endif
