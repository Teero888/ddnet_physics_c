#include "../src/gamecore.h"
#include "map_loader.h"
#include <stdio.h>
#include <time.h>

clock_t timer_start() { return clock(); }

double timer_end(clock_t start_time) {
  clock_t end_time = clock();
  return (double)(end_time - start_time) / CLOCKS_PER_SEC;
}

#define TICKS 1000000

int main() {
  SMapData Collision = load_map("run_blue.map");
  if (!Collision.m_GameLayer.m_pData)
    return 1;

  SConfig Config;
  init_config(&Config);

  SWorldCore World;
  wc_init(&World, &Collision, &Config);

  SCharacterCore *pChar = wc_add_character(&World);
  SPlayerInput Input = {};
  Input.m_Direction = 1;

  clock_t start_time = timer_start();
  for (int i = 0; i < TICKS; ++i) {
    Input.m_Direction = World.m_GameTick % 3 - 1;
    Input.m_Jump = World.m_GameTick % 2;
    Input.m_Hook = World.m_GameTick % 2;
    Input.m_TargetX = World.m_GameTick % 11 - 5;
    Input.m_TargetY = -World.m_GameTick % 11 - 5;

    cc_on_input(pChar, &Input);
    wc_tick(&World);
  }
  double elapsed_time = timer_end(start_time);
  printf("Took %.6f seconds for %d ticks\n", elapsed_time, TICKS);

  wc_free(&World);
  free_map_data(&Collision);

  return 0;
}
