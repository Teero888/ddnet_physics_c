#ifndef DDNET_PHYSICS_SETTINGS_H
#define DDNET_PHYSICS_SETTINGS_H

#include <ddnet_map_loader.h>
#include <ddnet_physics/gamecore.h>

typedef struct MapSettingsTarget {
  SConfig *m_pConfig;
  STuningParams *m_pTunings;
  SSwitch *m_pSwitches;
  bool *m_pGrenadeDoubleExplosion;
  bool *m_pUniqueRace;
  bool m_FastcapGameType;
  int m_NumSwitches;
} SMapSettingsTarget;

void apply_map_settings(const map_data_t *pMap, SMapSettingsTarget *pTarget);

#endif // DDNET_PHYSICS_SETTINGS_H
