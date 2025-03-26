#include "../src/gamecore.h"
#include "collision.h"
#include "map_loader.h"
#include "vmath.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

clock_t timer_start() { return clock(); }

double timer_end(clock_t start_time) {
  clock_t end_time = clock();
  return (double)(end_time - start_time) / CLOCKS_PER_SEC;
}

void format_int(long long num, char *result) {
  char buffer[50];
  sprintf(buffer, "%lld", num);

  int len = strlen(buffer);
  int newLen = len + (len - 1) / 3;
  int j = newLen - 1;
  result[newLen] = '\0';

  for (int i = len - 1, k = 0; i >= 0; i--, k++) {
    result[j--] = buffer[i];
    if (k % 3 == 2 && i != 0) {
      result[j--] = ',';
    }
  }
}

#define ITERATIONS 1000000

int main(void) {
  SCollision Collision;
  if (!init_collision(&Collision, "tests/maps/ctf1.map"))
    return 1;

  vec2 Pos;
  vec2 Vel;
  bool Grounded = false;
  clock_t start_time = timer_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    Pos = vec2_init(320.0f, 320.0f);
    Vel = vec2_init(320.0f, 320.0f);
    move_box(&Collision, &Pos, &Vel, vec2_init(0.0f, 0.0f), &Grounded);
  }
  printf("Pos: %f, %f; Vel: %f, %f\n", Pos.x, Pos.y, Vel.x, Vel.y);
  double elapsed_time = timer_end(start_time);

  char aBuf[32];
  format_int(ITERATIONS, aBuf);
  printf("Took %.6f seconds for %s iterations\n", elapsed_time, aBuf);
  format_int((float)ITERATIONS / elapsed_time, aBuf);
  printf("Resulting in %s move_box calls per second\n", aBuf);

  free_collision(&Collision);

  return 0;
}
