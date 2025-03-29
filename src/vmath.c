#include "vmath.h"

extern inline vec2 vec2_init(float x, float y);
extern inline vec2 vfmul(vec2 a, float b);
extern inline vec2 vfdiv(vec2 a, float b);
extern inline vec2 vfadd(vec2 a, float b);
extern inline vec2 vvadd(vec2 a, vec2 b);
extern inline vec2 vvsub(vec2 a, vec2 b);
extern inline float vdot(vec2 a, vec2 b);
extern inline float vlength(vec2 a);
extern inline float vdistance(vec2 a, vec2 b);
extern inline float vsqlength(vec2 a);
extern inline float vsqdistance(vec2 a, vec2 b);
extern inline vec2 vnormalize(vec2 a);
extern inline vec2 vnormalize_nomask(vec2 a);
extern inline bool vvcmp(vec2 a, vec2 b);
extern inline int round_to_int(float f);
extern inline float fclamp(float n, float a, float b);
extern inline vec2 vvfmix(vec2 a, vec2 b, float t);
extern inline vec2 vdirection(float angle);
extern inline bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                         vec2 target_point, vec2 *out_pos);
