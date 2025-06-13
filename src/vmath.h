#ifndef LIB_VMATH_H
#define LIB_VMATH_H

#include <immintrin.h>
#include <math.h>
#include <stdbool.h>

// #ifndef ALIGN
// #define ALIGN(x) __attribute__((aligned(x)))
// #endif
#ifndef NO_INLINE
#define NO_INLINE __attribute__((noinline))
#endif

typedef __m128 mvec2;
#define CTVEC2(x, y) {x, y, 0.f, 0.f}
typedef struct {
  unsigned int x, y;
} uivec2;

#define PI 3.14159265358979323846f

// Initialize a vector with x and y components
static inline mvec2 vec2_init(float x, float y) { return _mm_set_ps(0.0f, 0.0f, y, x); }

// Add x value to vector
static inline mvec2 vadd_x(mvec2 v, float x_val) { return _mm_add_ps(v, _mm_set_ps(0.0f, 0.0f, 0.0f, x_val)); }

// Subtract x value from vector
static inline mvec2 vsub_x(mvec2 v, float x_val) { return _mm_sub_ps(v, _mm_set_ps(0.0f, 0.0f, 0.0f, x_val)); }

// Add y value to vector
static inline mvec2 vadd_y(mvec2 v, float y_val) { return _mm_add_ps(v, _mm_set_ps(0.0f, 0.0f, y_val, 0.0f)); }

// Subtract y value from vector
static inline mvec2 vsub_y(mvec2 v, float y_val) { return _mm_sub_ps(v, _mm_set_ps(0.0f, 0.0f, y_val, 0.0f)); }

// Get x component
static inline float vgetx(mvec2 v) { return _mm_cvtss_f32(v); }

// Get y component
static inline float vgety(mvec2 v) { return _mm_cvtss_f32(_mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1))); }

// Set x component
static inline mvec2 vsetx(mvec2 v, float x) { return _mm_move_ss(v, _mm_set_ss(x)); }

// Set y component
static inline mvec2 vsety(mvec2 v, float y) {
  __m128 y_vec = _mm_set_ps(0.0f, 0.0f, y, 0.0f);
  return _mm_blend_ps(v, y_vec, 0x2); // SSE4.1
}

// Vector addition
static inline mvec2 vvadd(mvec2 a, mvec2 b) { return _mm_add_ps(a, b); }

// Vector subtraction
static inline mvec2 vvsub(mvec2 a, mvec2 b) { return _mm_sub_ps(a, b); }

// Component-wise multiplication
static inline mvec2 vvmul(mvec2 a, mvec2 b) { return _mm_mul_ps(a, b); }

// Scalar multiplication
static inline mvec2 vfmul(mvec2 a, float b) { return _mm_mul_ps(a, _mm_set1_ps(b)); }

// Scalar addition
static inline mvec2 vfadd(mvec2 a, float b) { return _mm_add_ps(a, _mm_set1_ps(b)); }

// Scalar division
static inline mvec2 vfdiv(mvec2 a, float b) { return _mm_div_ps(a, _mm_set1_ps(b)); }

// Dot product
static inline float vdot(mvec2 a, mvec2 b) {
  __m128 mul = _mm_mul_ps(a, b);
  __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1, 1, 1, 1));
  __m128 sum = _mm_add_ps(mul, shuf);
  return _mm_cvtss_f32(sum);
}

// Squared length
static inline float vsqlength(mvec2 a) { return vdot(a, a); }

// Length
static inline float vlength(mvec2 a) { return sqrt(vsqlength(a)); }

// Compare vectors
static inline bool vvcmp(mvec2 a, mvec2 b) {
  __m128 cmp = _mm_cmpeq_ps(a, b);
  return (_mm_movemask_ps(cmp) & 0x3) == 0x3; // Check first 2 components
}

// Squared distance
static inline float vsqdistance(mvec2 a, mvec2 b) {
  mvec2 diff = vvsub(a, b);
  return vsqlength(diff);
}

// Distance
static inline float vdistance(mvec2 a, mvec2 b) { return sqrt(vsqdistance(a, b)); }

static inline mvec2 vnormalize(mvec2 a) {
  const float divisor = vlength(a);
  const float mask = (float)(divisor != 0.0f);
  const float l = mask * (1.0f / (divisor + (1.0f - mask)));
  return vfmul(a, l);
}

static inline mvec2 vnormalize_nomask(mvec2 a) {
  const float l = 1.0f / vlength(a);
  return vfmul(a, l);
}

static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int iclamp(int num, int low, int high) { return num < low ? low : num > high ? high : num; }

// Clamp value
static inline float fclamp(float n, float a, float b) {
  __m128 val = _mm_set_ss(n);
  __m128 min = _mm_set_ss(a);
  __m128 max = _mm_set_ss(b);
  val = _mm_max_ps(val, min);
  val = _mm_min_ps(val, max);
  return _mm_cvtss_f32(val);
}

// Linear interpolation between vectors
static inline mvec2 vvfmix(mvec2 a, mvec2 b, float t) {
  mvec2 diff = vvsub(b, a);
  return vvadd(a, vfmul(diff, t));
}

// Create direction vector from angle (in radians)
static inline mvec2 vdirection(float angle) { return vec2_init(cosf(angle), sinf(angle)); }

static inline bool closest_point_on_line(mvec2 line_pointA, mvec2 line_pointB, mvec2 target_point,
                                         mvec2 *out_pos) {
  mvec2 AB = vvsub(line_pointB, line_pointA);
  float sq_mag_AB = vdot(AB, AB);
  if (sq_mag_AB > 0.0f) {
    mvec2 AP = vvsub(target_point, line_pointA);
    float t = vdot(AP, AB) / sq_mag_AB;
    *out_pos = vvadd(line_pointA, vfmul(AB, fclamp(t, 0.0f, 1.0f)));
    return true;
  }
  return false;
}

#endif
