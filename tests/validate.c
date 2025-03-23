
#include "../src/gamecore.h"
#include "data.h"
#include "map_loader.h"
#include "vmath.h"
#include <stdio.h>

#define TICKS 1000000

#define SIZE(x) sizeof(x) / sizeof(x[0])

int main(void) {
  SMapData Collision = load_map("run_blue.map");
  if (!Collision.m_GameLayer.m_pData)
    return 1;

  SConfig Config;
  init_config(&Config);

  SWorldCore World;
  wc_init(&World, &Collision, &Config);

  SValidateState *pData = aRunBlueNoWpns;
  int DataSize = SIZE(aRunBlueNoWpns);

  SCharacterCore *pChar = wc_add_character(&World);
  vec2 PreviousVel;

  bool Failed = false;
  printf("StartTick: %d\n", pData[0].m_GameTick);
  printf("DataSize: %d\n", DataSize);
  for (int i = 0; i < pData[0].m_GameTick + DataSize; ++i) {
    if (i >= pData[0].m_GameTick)
      cc_on_input(pChar, &pData[i - pData[0].m_GameTick].m_Input);
    PreviousVel = pChar->m_Vel;
    wc_tick(&World);
    if (i >= pData[0].m_GameTick) {
      int Tick = i - pData[0].m_GameTick;
      if (!vvcmp(pData[Tick].m_Pos, pChar->m_Pos)) {
        printf("Validation failed at tick %d\n", i);
        printf("Expected State:\n"
               "\tPos: %f, %f\n"
               "\tVel: %f, %f\n"
               "Found State: \n"
               "\tPos: %f, %f\n"
               "\tVel: %f, %f\n",
               pData[Tick].m_Pos.x, pData[Tick].m_Pos.y, pData[Tick].m_Vel.x,
               pData[Tick].m_Vel.y, pChar->m_Pos.x, pChar->m_Pos.y,
               pChar->m_Vel.x, pChar->m_Vel.y);
        printf("Previous State:\n"
               "\tPos: %f, %f\n"
               "\tVel: %f, %f\n",
               pChar->m_PrevPos.x, pChar->m_PrevPos.y, PreviousVel.x,
               PreviousVel.y);
        Failed = true;
        break;
      }
    }
  }
  if (!Failed) {
    printf("Validation successful.\n");
  }

  wc_free(&World);
  free_map_data(&Collision);

  return 0;
}
