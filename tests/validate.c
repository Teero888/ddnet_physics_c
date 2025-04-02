#include "../src/collision.h"
#include "../src/gamecore.h"
#include "../src/vmath.h"
#include "data.h"
#include <stdio.h>

#define TICKS 1000000

#define SIZE(x) sizeof(x) / sizeof(x[0])

typedef struct {
  const char *m_Name;
  const char *m_Description;
  const SValidation *m_pValidationData;
} STest;

static const STest s_aTests[] = {
    (STest){"jump",
            "simple jumping up and down on ctf0 from top left red spawn",
            &s_JumpTest},
    (STest){"direction",
            "simple direction changes on ctf0 from top left red spawn",
            &s_DirectionTest},
    (STest){"hook", "moving using just hook and no other inputs", &s_HookTest},
    /*     (STest){"grenade",
                "moves to the first grenade on the left side of ctf0, picks it
       up " "and shoots randomly without moving", &s_GrenadeTest}, */
    (STest){"stopper physics", "tests the stoppers on a random stopper map",
            &s_StopperTest},

    /*     (STest){"test run", "a simple real run example", &s_TestRun}, */
};

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
    vec2 PreviousVel;

    bool Failed = false;
    for (int i = 0; i < pData->m_StartTick + pData->m_Ticks; ++i) {
      if (i >= pData->m_StartTick) {
        cc_on_input(pChar,
                    &pData->m_vStates[0][i - pData->m_StartTick].m_Input);
      }
      PreviousVel = pChar->m_Vel;
      wc_tick(&World);
      if (i >= pData->m_StartTick) {
        int Tick = i - pData->m_StartTick;
        if (!vvcmp(pData->m_vStates[0][Tick].m_Pos, pChar->m_Pos) ||
            !vvcmp(pData->m_vStates[0][Tick].m_Vel, pChar->m_Vel)) {
          printf("Test '%s' failed at step %d\n", s_aTests[Test].m_Name, Tick);
          printf("Expected State:\n"
                 "\tPos: %a, %a\n"
                 "\tVel: %a, %a\n"
                 "Found State: \n"
                 "\tPos: %a, %a\n"
                 "\tVel: %a, %a\n",
                 vgetx(pData->m_vStates[0][Tick].m_Pos),
                 vgety(pData->m_vStates[0][Tick].m_Pos),
                 vgetx(pData->m_vStates[0][Tick].m_Vel),
                 vgety(pData->m_vStates[0][Tick].m_Vel), vgetx(pChar->m_Pos),
                 vgety(pChar->m_Pos), vgetx(pChar->m_Vel), vgety(pChar->m_Vel));
          printf("Previous State:\n"
                 "\tPos: %a, %a\n"
                 "\tVel: %a, %a\n",
                 vgetx(pChar->m_PrevPos), vgety(pChar->m_PrevPos),
                 vgetx(PreviousVel), vgety(PreviousVel));
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
      break;
  }

  return 0;
}
