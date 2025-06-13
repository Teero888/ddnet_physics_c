#include "../include/collision.h"
#include "../include/gamecore.h"
#include "../include/vmath.h"
#include "data.h"
#include <stdio.h>

#define TICKS 1000000

#define SIZE(x) sizeof(x) / sizeof(x[0])

int main(void) {
  for (int Test = 0; (unsigned long)Test < SIZE(s_aTests); ++Test) {
    const SValidation *pData = s_aTests[Test].m_pValidationData;
    char aMapPath[64];
    snprintf(aMapPath, 64, "maps/%s", pData->m_aMapName);
    SCollision Collision;
    if (!init_collision(&Collision, aMapPath))
      return 1;

    SConfig Config;
    init_config(&Config);

    SWorldCore World;
    wc_init(&World, &Collision, &Config);

    SCharacterCore *pChar = wc_add_character(&World);
    mvec2 PreviousVel;

    bool Failed = false;
    for (int i = 0; i < pData->m_StartTick + pData->m_Ticks; ++i) {
      if (i >= pData->m_StartTick) {
        cc_on_input(pChar, &pData->m_vStates[0][i - pData->m_StartTick].m_Input);
      }
      PreviousVel = pChar->m_Vel;
      wc_tick(&World);
      if (i >= pData->m_StartTick) {
        int Tick = i - pData->m_StartTick;
        if (!vvcmp(pData->m_vStates[0][Tick].m_Pos, pChar->m_Pos) ||
            !vvcmp(pData->m_vStates[0][Tick].m_Vel, pChar->m_Vel)) {
          printf("Test '%s' failed at step %d, i = %d\n", s_aTests[Test].m_Name, Tick, i);
          printf("Expected State:\n"
                 "\tPos: %.10f, %.10f\n"
                 "\tVel: %.10f, %.10f\n"
                 "Found State: \n"
                 "\tPos: %.10f, %.10f\n"
                 "\tVel: %.10f, %.10f\n",
                 vgetx(pData->m_vStates[0][Tick].m_Pos), vgety(pData->m_vStates[0][Tick].m_Pos),
                 vgetx(pData->m_vStates[0][Tick].m_Vel), vgety(pData->m_vStates[0][Tick].m_Vel),
                 vgetx(pChar->m_Pos), vgety(pChar->m_Pos), vgetx(pChar->m_Vel), vgety(pChar->m_Vel));
          printf("Previous State:\n"
                 "\tPos: %.10f, %.10f\n"
                 "\tVel: %.10f, %.10f\n",
                 vgetx(pChar->m_PrevPos), vgety(pChar->m_PrevPos), vgetx(PreviousVel), vgety(PreviousVel));
          Failed = true;
          break;
        }
      }
    }
    if (!Failed) {
      printf("Test '%s' passed.\n", s_aTests[Test].m_Name);
    }

    wc_free(&World);
    free_collision(&Collision);
    if (Failed)
      return 1;
  }

  return 0;
}
