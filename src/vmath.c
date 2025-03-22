#include "vmath.h"

vec2 vfmul(vec2 a, float b) { return (vec2){a.x * b, a.y * b}; }
vec2 vfdiv(vec2 a, float b) { return (vec2){a.x / b, a.y / b}; }
vec2 vfadd(vec2 a, float b) { return (vec2){a.x + b, a.y + b}; }
vec2 vvadd(vec2 a, vec2 b) { return (vec2){a.x + b.x, a.y + b.y}; }
vec2 vvsub(vec2 a, vec2 b) { return (vec2){a.x - b.x, a.y - b.y}; }
float vdot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; };
float vlength(vec2 a) { return sqrt(a.x * a.x + a.y * a.y); }
float vdistance(vec2 a, vec2 b) { return vlength(vvsub(a, b)); }
vec2 vnormalize(vec2 a) { return vfdiv(a, vlength(a)); }

bool vvcmp(vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }

int round_to_int(float f) { return f > 0 ? (int)(f + 0.5f) : (int)(f - 0.5f); }

float fclamp(float n, float a, float b) { return n > b ? b : n < a ? a : n; }
int iclamp(int n, int a, int b) { return n > b ? b : n < a ? a : n; }

vec2 vvfmix(vec2 a, vec2 b, float t) { return vvadd(a, vfmul(vvsub(b, a), t)); }
vec2 vdirection(float angle) { return vec2(cos(angle), sin(angle)); }

bool closest_point_on_line(vec2 line_pointA, vec2 line_pointB,
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
