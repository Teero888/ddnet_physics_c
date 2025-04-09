#ifndef LIB_VMATH_H
#define LIB_VMATH_H

#include <immintrin.h>
#include <math.h>
#include <stdbool.h>

#ifndef ALIGN
#define ALIGN(x) __attribute__((aligned(x)))
#endif
typedef __m128 vec2;
#define CTVEC2(x, y) {x, y, 0.f, 0.f}
typedef struct {
  unsigned int x, y;
} uivec2;

#define PI 3.14159265358979323846f

// Initialize a vector with x and y components
inline vec2 vec2_init(float x, float y) { return _mm_set_ps(0.0f, 0.0f, y, x); }

// Add x value to vector
inline vec2 vadd_x(vec2 v, float x_val) {
  return _mm_add_ps(v, _mm_set_ps(0.0f, 0.0f, 0.0f, x_val));
}

// Subtract x value from vector
inline vec2 vsub_x(vec2 v, float x_val) {
  return _mm_sub_ps(v, _mm_set_ps(0.0f, 0.0f, 0.0f, x_val));
}

// Add y value to vector
inline vec2 vadd_y(vec2 v, float y_val) {
  return _mm_add_ps(v, _mm_set_ps(0.0f, 0.0f, y_val, 0.0f));
}

// Subtract y value from vector
inline vec2 vsub_y(vec2 v, float y_val) {
  return _mm_sub_ps(v, _mm_set_ps(0.0f, 0.0f, y_val, 0.0f));
}

// Get x component
inline float vgetx(vec2 v) { return _mm_cvtss_f32(v); }

// Get y component
inline float vgety(vec2 v) {
  return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
}

// Set x component
inline vec2 vsetx(vec2 v, float x) { return _mm_move_ss(v, _mm_set_ss(x)); }

// Set y component
inline vec2 vsety(vec2 v, float y) {
  __m128 y_vec = _mm_set_ps(0.0f, 0.0f, y, 0.0f);
  return _mm_blend_ps(v, y_vec, 0x2); // SSE4.1
}

// Vector addition
inline vec2 vvadd(vec2 a, vec2 b) { return _mm_add_ps(a, b); }

// Vector subtraction
inline vec2 vvsub(vec2 a, vec2 b) { return _mm_sub_ps(a, b); }

// Component-wise multiplication
inline vec2 vvmul(vec2 a, vec2 b) { return _mm_mul_ps(a, b); }

// Scalar multiplication
inline vec2 vfmul(vec2 a, float b) { return _mm_mul_ps(a, _mm_set1_ps(b)); }

// Scalar addition
inline vec2 vfadd(vec2 a, float b) { return _mm_add_ps(a, _mm_set1_ps(b)); }

// Scalar division
inline vec2 vfdiv(vec2 a, float b) { return _mm_div_ps(a, _mm_set1_ps(b)); }

// Dot product
inline float vdot(vec2 a, vec2 b) {
  __m128 mul = _mm_mul_ps(a, b);
  __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 1, 1, 1));
  __m128 sum = _mm_add_ps(mul, shuf);
  return _mm_cvtss_f32(sum);
}

// Squared length
inline float vsqlength(vec2 a) { return vdot(a, a); }

// Length
inline float vlength(vec2 a) { return sqrt(vsqlength(a)); }

// Compare vectors
inline bool vvcmp(vec2 a, vec2 b) {
  __m128 cmp = _mm_cmpeq_ps(a, b);
  return (_mm_movemask_ps(cmp) & 0x3) == 0x3; // Check first 2 components
}

// Squared distance
inline float vsqdistance(vec2 a, vec2 b) {
  vec2 diff = vvsub(a, b);
  return vsqlength(diff);
}

// Distance
inline float vdistance(vec2 a, vec2 b) { return sqrt(vsqdistance(a, b)); }

inline vec2 vnormalize(vec2 a) {
  const float divisor = vlength(a);
  const float mask = (float)(divisor != 0.0f);
  const float l = mask * (1.0f / (divisor + (1.0f - mask)));
  return vfmul(a, l);
}

inline vec2 vnormalize_nomask(vec2 a) {
  const float l = 1.0f / vlength(a);
  return vfmul(a, l);
}

inline int imax(int a, int b) { return a > b ? a : b; }
inline int imin(int a, int b) { return a < b ? a : b; }
inline int iclamp(int num, int low, int high) {
  return num < low ? low : num > high ? high : num;
}

// Clamp value
inline float fclamp(float n, float a, float b) {
  __m128 val = _mm_set_ss(n);
  __m128 min = _mm_set_ss(a);
  __m128 max = _mm_set_ss(b);
  val = _mm_max_ps(val, min);
  val = _mm_min_ps(val, max);
  return _mm_cvtss_f32(val);
}

// Linear interpolation between vectors
inline vec2 vvfmix(vec2 a, vec2 b, float t) {
  vec2 diff = vvsub(b, a);
  return vvadd(a, vfmul(diff, t));
}

// Create direction vector from angle (in radians)
inline vec2 vdirection(float angle) {
  return vec2_init(cosf(angle), sinf(angle));
}

inline bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                  vec2 target_point, vec2 *out_pos) {
  vec2 AB = vvsub(line_pointB, line_pointA);
  float sq_mag_AB = vdot(AB, AB);
  if (sq_mag_AB > 0.0f) {
    vec2 AP = vvsub(target_point, line_pointA);
    float t = vdot(AP, AB) / sq_mag_AB;
    *out_pos = vvadd(line_pointA, vfmul(AB, fclamp(t, 0.0f, 1.0f)));
    return true;
  }
  return false;
}

#endif
