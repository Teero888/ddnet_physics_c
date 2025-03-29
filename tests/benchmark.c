#include "../src/gamecore.h"
#include "collision.h"
#include "data.h"
#include "utils.h"
#include <stdio.h>

#define ITERATIONS 333
#define TICKS ITERATIONS * 3000

int main(void) {
  SCollision Collision;
  if (!init_collision(&Collision, "tests/maps/test_run.map"))
    return 1;
  SConfig Config;
  init_config(&Config);

  SWorldCore StartWorld;
  wc_init(&StartWorld, &Collision, &Config);
  wc_add_character(&StartWorld);
  for (int t = 0; t < 50; ++t)
    wc_tick(&StartWorld);

  clock_t start_time = timer_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    SWorldCore World = (SWorldCore){};
    wc_copy_world(&World, &StartWorld);
    for (int t = 0; t < 3000; ++t) {
      cc_on_input(&World.m_pCharacters[0], &s_TestRun.m_vStates[0][t].m_Input);
      wc_tick(&World);
    }
    wc_free(&World);
    // printf("TeePos: %d, %d\n", (int)pChar->m_Pos.x >> 5,
    //        (int)pChar->m_Pos.y >> 5);
  }
  double elapsed_time = timer_end(start_time);

  char aBuf[32];
  format_int(TICKS, aBuf);
  printf("Took %.6f seconds for %s ticks\n", elapsed_time, aBuf);
  format_int((float)TICKS / elapsed_time, aBuf);
  printf("Resulting in %s TPS\n", aBuf);

  wc_free(&StartWorld);
  free_collision(&Collision);

  return 0;
}
