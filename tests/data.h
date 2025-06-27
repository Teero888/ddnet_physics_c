#include "../include/gamecore.h"
#include "../include/vmath.h"

// Each test has a maximum on one minute playtime and 4 players for now
typedef struct {
  SPlayerInput m_Input;
  mvec2 m_Pos;
  mvec2 m_Vel;
  mvec2 m_HookPos;
  int m_Reload;
} SPlayerState;
typedef struct {
  char m_aMapName[32];
  int m_StartTick;
  int m_NumCharacters;
  int m_Ticks;
  SPlayerState m_vStates[4][3000];
} SValidation;

typedef struct {
  const char *m_Name;
  const char *m_Description;
  const SValidation *m_pValidationData;
} STest;


static const STest s_aTests[] = {
  // (STest){"hook bug", "a bug that occured while playing run_antibuguse", &s_AntiHookBug},
};
