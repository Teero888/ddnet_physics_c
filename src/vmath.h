#ifndef LIB_VMATH_H
#define LIB_VMATH_H
#include <math.h>
#include <stdbool.h>
#include <xmmintrin.h>

#ifndef ALIGN
#define ALIGN(x) __attribute__((aligned(x)))
#endif

typedef union vec2 {
  struct {
    float x, y;
  };
  float arr[2];
  __m128 simd;
} ALIGN(16) vec2;

#define PI 3.14159265358979323846

// Only use this for compile time stuff
#define CTVEC2(x, y) ((vec2){{x, y}})

inline vec2 vec2_init(float x, float y) {
  return (vec2){.simd = _mm_set_ps(0.0f, 0.0f, y, x)};
}

inline vec2 vfmul(vec2 a, float b) { return vec2_init(a.x * b, a.y * b); }

// saves ~30ms on the benchmark
inline vec2 vfdiv(vec2 a, float b) {
  __m128 scalar = _mm_set1_ps(b);
  return (vec2){.simd = _mm_div_ps(a.simd, scalar)};
}

inline vec2 vfadd(vec2 a, float b) { return vec2_init(a.x + b, a.y + b); }
inline vec2 vvadd(vec2 a, vec2 b) { return vec2_init(a.x + b.x, a.y + b.y); }
inline vec2 vvsub(vec2 a, vec2 b) { return vec2_init(a.x - b.x, a.y - b.y); }
inline float vdot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; };
inline float vlength(vec2 a) { return sqrt(a.x * a.x + a.y * a.y); }
inline float vdistance(vec2 a, vec2 b) { return vlength(vvsub(a, b)); }

// this has to be this exact else it will generate super small differences
// between these physics and the ddnet ones
inline vec2 vnormalize(vec2 a) {
  const float divisor = vlength(a);
  const float mask = (float)(divisor != 0.0f);
  const float l = mask * (1.0f / (divisor + (1.0f - mask)));
  return vec2_init(a.x * l, a.y * l);
}

inline bool vvcmp(vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }

inline int round_to_int(float f) {
  return f > 0 ? (int)(f + 0.5f) : (int)(f - 0.5f);
}

inline float fclamp(float n, float a, float b) {
  return n > b ? b : n < a ? a : n;
}

inline int ___iclamp(int n, int a, int b) { return n > b ? b : n < a ? a : n; }

#ifdef NO_COLLISION_CLAMP
#define iclamp(n, a, b) (n)
#warning "NO_COLLISION_CLAMP is activated. Do not let your tee go out of bounds"
#else
#define iclamp(n, a, b) ___iclamp(n, a, b)
#endif

// saves ~20ms on the benchmark compared to non-simd usage within ddnet physics
inline vec2 vvfmix(vec2 a, vec2 b, float t) {
  __m128 t_vec = _mm_set1_ps(t);
  __m128 diff = _mm_sub_ps(b.simd, a.simd);
  __m128 scaled_diff = _mm_mul_ps(diff, t_vec);
  return (vec2){.simd = _mm_add_ps(a.simd, scaled_diff)};
}

inline vec2 vdirection(float angle) {
  return vec2_init(cos(angle), sin(angle));
}

inline bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
                                  vec2 target_point, vec2 *out_pos) {
  vec2 AB = vvsub(line_pointB, line_pointA);
  float SquaredMagnitudeAB = vdot(AB, AB);
  if (SquaredMagnitudeAB > 0) {
    vec2 AP = vvsub(target_point, line_pointA);
    float APdotAB = vdot(AP, AB);
    float t = APdotAB / SquaredMagnitudeAB;
    *out_pos = vvadd(line_pointA, vfmul(AB, fclamp(t, (float)0, (float)1)));
    return true;
  }
  return false;
}

#endif // LIB_VMATH_H
