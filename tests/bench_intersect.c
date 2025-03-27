#include "collision.h"
#include "utils.h"
#include "vmath.h"
#include <stdio.h>

#define ITERATIONS 1000000

int main(void) {
  SCollision Collision;
  if (!init_collision(&Collision, "tests/maps/ctf1.map"))
    return 1;

  vec2 Pos0 = vec2_init(320.0f, 320.0f);
  vec2 Pos1 = vec2_init(640.0f, 640.0f);
  vec2 OutCol;
  clock_t start_time = timer_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    intersect_line_tele_hook(&Collision, Pos0, Pos1, &OutCol, NULL, false);
  }
  // printf("OutPos: %f, %f\n", OutCol.x, OutCol.y);
  double elapsed_time = timer_end(start_time);

  char aBuf[32];
  format_int(ITERATIONS, aBuf);
  printf("Took %.6f seconds for %s iterations\n", elapsed_time, aBuf);
  format_int((float)ITERATIONS / elapsed_time, aBuf);
  printf("Resulting in %s intersect_line_tele_hook calls per second\n", aBuf);

  free_collision(&Collision);

  return 0;
}
