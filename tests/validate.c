
#include "../src/gamecore.h"
#include "data.h"
#include "map_loader.h"
#include "vmath.h"
#include <stdio.h>

#define TICKS 1000000

#define SIZE(x) sizeof(x) / sizeof(x[0])

typedef struct {
  const char *m_Description;
  const SValidation *m_pValidationData;
} STest;

static const STest s_aTests[] = {
    (STest){"run_blue jumping up and down", &s_JumpTest},
    (STest){"run_blue with simple hook movements", &s_HookTest},
    (STest){"run_blue with simple grenade movements", &s_GrenadeTest},
};

int main(void) {
  for (int Test = 0; (unsigned long)Test < SIZE(s_aTests); ++Test) {
    const SValidation *pData = s_aTests[Test].m_pValidationData;
    SMapData Collision = load_map(pData->m_aMapName, true);
    if (!Collision.m_GameLayer.m_pData)
      return 1;

    SConfig Config;
    init_config(&Config);

    SWorldCore World;
    wc_init(&World, &Collision, &Config);

    SCharacterCore *pChar = wc_add_character(&World);
    vec2 PreviousVel;

    bool Failed = false;
    for (int i = 0; i < pData->m_StartTick + 3000; ++i) {
      if (i >= pData->m_StartTick) {
        cc_on_input(pChar,
                    &pData->m_vStates[0][i - pData->m_StartTick].m_Input);
      }
      PreviousVel = pChar->m_Vel;
      wc_tick(&World);
      if (i >= pData->m_StartTick) {
        int Tick = i - pData->m_StartTick;
        if (!vvcmp(pData->m_vStates[0][Tick].m_Pos, pChar->m_Pos)) {
          printf("Test '%s' failed at step %d\n", s_aTests[Test].m_Description,
                 Tick);
          printf("Expected State:\n"
                 "\tPos: %f, %f\n"
                 "\tVel: %f, %f\n"
                 "Found State: \n"
                 "\tPos: %f, %f\n"
                 "\tVel: %f, %f\n",
                 pData->m_vStates[0][Tick].m_Pos.x,
                 pData->m_vStates[0][Tick].m_Pos.y,
                 pData->m_vStates[0][Tick].m_Vel.x,
                 pData->m_vStates[0][Tick].m_Vel.y, pChar->m_Pos.x,
                 pChar->m_Pos.y, pChar->m_Vel.x, pChar->m_Vel.y);
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
      printf("Test '%s' passed.\n", s_aTests[Test].m_Description);
    }

    wc_free(&World);
    free_map_data(&Collision);
  }

  return 0;
}
