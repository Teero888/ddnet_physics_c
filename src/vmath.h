#ifndef LIB_VMATH_H
#define LIB_VMATH_H

#include <math.h>
#include <stdbool.h>
#include <xmmintrin.h>

#define ALIGN(x) __attribute__((aligned(x)))

typedef union vec2 {
  struct {
    float x, y;
  };
  float arr[2];
  __m128 simd;
} ALIGN(16) vec2;

#define VZERO ((vec2){.simd = _mm_setzero_ps()})
#define PI 3.14159265358979323846f

#define ALWAYS_INLINE __attribute__((always_inline)) inline

ALWAYS_INLINE vec2 vec2_init(float x, float y) {
  return (vec2){.simd = _mm_set_ps(0.0f, 0.0f, y, x)};
}

ALWAYS_INLINE vec2 vfmul(vec2 a, float b) {
  const __m128 scalar = _mm_set1_ps(b);
  return (vec2){.simd = _mm_mul_ps(a.simd, scalar)};
}

ALWAYS_INLINE vec2 vfdiv(vec2 a, float b) {
  const __m128 inv_b = _mm_rcp_ss(_mm_set_ss(b));
  return vfmul(a, _mm_cvtss_f32(inv_b));
}

ALWAYS_INLINE vec2 vvadd(vec2 a, vec2 b) {
  return (vec2){.simd = _mm_add_ps(a.simd, b.simd)};
}

ALWAYS_INLINE vec2 vfadd(vec2 a, float b) { return vvadd(a, vec2_init(b, b)); }

ALWAYS_INLINE vec2 vvsub(vec2 a, vec2 b) {
  return (vec2){.simd = _mm_sub_ps(a.simd, b.simd)};
}

ALWAYS_INLINE float vdot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; };

ALWAYS_INLINE float vlength(vec2 a) {
  const __m128 sq = _mm_mul_ps(a.simd, a.simd);
  const __m128 rsq = _mm_rsqrt_ss(
      _mm_add_ss(_mm_shuffle_ps(sq, sq, 0x4E), _mm_shuffle_ps(sq, sq, 0x11)));
  const float len = 1.0f / _mm_cvtss_f32(rsq);
  return len * (1.5f - 0.5f * len * len * _mm_cvtss_f32(sq));
}

ALWAYS_INLINE float vdistance(vec2 a, vec2 b) { return vlength(vvsub(a, b)); }

ALWAYS_INLINE vec2 vnormalize(vec2 a) {
  const __m128 sq = _mm_mul_ps(a.simd, a.simd);
  const __m128 rsq = _mm_rsqrt_ps(
      _mm_add_ps(_mm_shuffle_ps(sq, sq, 0x4E), _mm_shuffle_ps(sq, sq, 0x11)));
  return (vec2){.simd = _mm_mul_ps(a.simd, rsq)};
}

ALWAYS_INLINE bool vvcmp(vec2 a, vec2 b) {
  const __m128 eq = _mm_cmpeq_ps(a.simd, b.simd);
  return (_mm_movemask_ps(eq) & 0x3) == 0x3;
}

ALWAYS_INLINE int round_to_int(float f) {
  return _mm_cvtss_si32(_mm_set_ss(f + 0.5f));
}

ALWAYS_INLINE float fclamp(float n, float a, float b) {
  const __m128 val = _mm_set_ss(n);
  const __m128 lo = _mm_set_ss(a);
  const __m128 hi = _mm_set_ss(b);
  const __m128 clamped = _mm_min_ss(_mm_max_ss(val, lo), hi);
  return _mm_cvtss_f32(clamped);
}

// Branchless integer clamp (only when using -ffast-math tho which i don't)
ALWAYS_INLINE int iclamp(int n, int a, int b) {
  const int t = n < a ? a : n;
  return t > b ? b : t;
}

ALWAYS_INLINE vec2 vvfmix(vec2 a, vec2 b, float t) {
  const __m128 tvec = _mm_set1_ps(t);
  const __m128 delta = _mm_sub_ps(b.simd, a.simd);
  return (vec2){.simd = _mm_add_ps(a.simd, _mm_mul_ps(delta, tvec))};
}

ALWAYS_INLINE vec2 vdirection(float angle) {
  float sin_theta = sin(angle), cos_theta = cos(angle);
  return vec2_init(cos_theta, sin_theta);
}

ALWAYS_INLINE bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                         vec2 target_point, vec2 *out_pos) {
  const vec2 AB = vvsub(line_pointB, line_pointA);
  const float sq_mag_ab = vdot(AB, AB);

  if (sq_mag_ab > 1e-8f) {
    const vec2 AP = vvsub(target_point, line_pointA);
    const float t = fclamp(vdot(AP, AB) / sq_mag_ab, 0.0f, 1.0f);
    *out_pos = vvadd(line_pointA, vfmul(AB, t));
    return true;
  }
  return false;
}

#endif // LIB_VMATH_H
