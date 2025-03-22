#ifndef LIB_VMATH_H
#define LIB_VMATH_H

#include <math.h>
#include <stdbool.h>

typedef struct vec2 {
  float x, y;
} vec2;

#define VZERO ((vec2){0, 0})
#define PI 3.14159265358979323846

#define vec2(a, b) ((vec2){a, b})

inline vec2 vfmul(vec2 a, float b);
inline vec2 vfdiv(vec2 a, float b);
inline vec2 vfadd(vec2 a, float b);
inline vec2 vvadd(vec2 a, vec2 b);
inline vec2 vvsub(vec2 a, vec2 b);
inline float vdot(vec2 a, vec2 b);
inline float vlength(vec2 a);
inline float vdistance(vec2 a, vec2 b);
inline vec2 vnormalize(vec2 a);
inline bool vvcmp(vec2 a, vec2 b);
inline int round_to_int(float f);
inline float fclamp(float n, float a, float b);
inline int iclamp(int n, int a, int b);
inline vec2 vvfmix(vec2 a, vec2 b, float t);
inline vec2 vdirection(float angle);
inline bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                  vec2 target_point, vec2 *out_pos);

#endif // LIB_VMATH_H
