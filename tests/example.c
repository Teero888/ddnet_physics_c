#include <stdio.h>

#include "../src/collision.h"
#include "../src/gamecore.h"

int main(void) {
  SCollision Collision;
  if (!init_collision(&Collision, "maps/ctf0.map"))
    return 1;

  SConfig Config;
  init_config(&Config);

  SWorldCore World;
  wc_init(&World, &Collision, &Config);

  SCharacterCore *pChar = wc_add_character(&World);
  SPlayerInput Input = {};
  Input.m_Direction = 1;

  for (int i = 0; i < 150; ++i) {
    cc_on_input(pChar, &Input);
    wc_tick(&World);
    printf("Pos:%.2f, %.2f, Vel: %.2f, %.2f\n", pChar->m_Pos.x, pChar->m_Pos.y,
           pChar->m_Vel.x, pChar->m_Vel.y);
  }

  wc_free(&World);
  free_collision(&Collision);

  return 0;
}
