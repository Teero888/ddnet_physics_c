#include "vmath.h"

extern inline vec2 vec2_init(float x, float y);
extern inline vec2 vadd_x(vec2 v, float x_val);
extern inline vec2 vsub_x(vec2 v, float x_val);
extern inline vec2 vadd_y(vec2 v, float y_val);
extern inline vec2 vsub_y(vec2 v, float y_val);
extern inline float vgetx(vec2 v);
extern inline float vgety(vec2 v);
extern inline vec2 vsetx(vec2 v, float x);
extern inline vec2 vsety(vec2 v, float y);
extern inline vec2 vvadd(vec2 a, vec2 b);
extern inline vec2 vvsub(vec2 a, vec2 b);
extern inline vec2 vvmul(vec2 a, vec2 b);
extern inline vec2 vfmul(vec2 a, float b);
extern inline vec2 vfadd(vec2 a, float b);
extern inline vec2 vfdiv(vec2 a, float b);
extern inline float vdot(vec2 a, vec2 b);
extern inline float vsqlength(vec2 a);
extern inline float vlength(vec2 a);
extern inline bool vvcmp(vec2 a, vec2 b);
extern inline float vsqdistance(vec2 a, vec2 b);
extern inline float vdistance(vec2 a, vec2 b);
extern inline vec2 vnormalize(vec2 a);
extern inline vec2 vnormalize_nomask(vec2 a);
extern inline int round_to_int(float f);
extern inline float fclamp(float n, float a, float b);
extern inline vec2 vvfmix(vec2 a, vec2 b, float t);
extern inline vec2 vdirection(float angle);
extern inline bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                         vec2 target_point, vec2 *out_pos);
