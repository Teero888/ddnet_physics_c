#include "settings.h"

#include <ddnet_physics/gamecore.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(Condition)                                                                                                                             \
  do {                                                                                                                                               \
    if (!(Condition)) {                                                                                                                              \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #Condition);                                                                \
      return 1;                                                                                                                                      \
    }                                                                                                                                                \
  } while (0)

static void init_test_tunings(STuningParams *pTunings) {
  memset(pTunings, 0, sizeof(STuningParams) * NUM_TUNE_ZONES);
  for (int Zone = 0; Zone < NUM_TUNE_ZONES; ++Zone) {
    pTunings[Zone].m_VelrampCurvature = 1.4f;
    pTunings[Zone].m_VelrampRange = 2000.0f;
    pTunings[Zone].m_PlayerCollision = 1.0f;
    pTunings[Zone].m_PlayerHooking = 1.0f;
  }
}

static int test_tunings(void) {
  char *apSettings[] = {
      "tune gravity 0.123",
      "TUNE_ZONE 7 ground_control_speed 13.37",
      "tune_zone 8 \"hook_length\" 444.44",
      "tune velramp_curvature 2; tune velramp_range 1000",
      "tune_zone 256 gravity 10",
      "tune unknown_parameter 123",
      "tune gravity nan",
      "mapbug grenade-doubleexplosion@ddnet.tw",
      "# tune gravity 20",
  };
  map_data_t Map = {
      .num_settings = (int)(sizeof(apSettings) / sizeof(apSettings[0])),
      .settings = apSettings,
  };
  STuningParams aTunings[NUM_TUNE_ZONES];
  init_test_tunings(aTunings);
  bool GrenadeDoubleExplosion = false;

  apply_map_settings(&Map, &(SMapSettingsTarget){
                               .m_pTunings = aTunings,
                               .m_pGrenadeDoubleExplosion = &GrenadeDoubleExplosion,
                           });

  CHECK(fabsf(aTunings[0].m_Gravity - 0.12f) < 0.00001f);
  CHECK(fabsf(aTunings[7].m_GroundControlSpeed - 13.37f) < 0.00001f);
  CHECK(fabsf(aTunings[8].m_HookLength - 444.44f) < 0.00001f);
  CHECK(fabsf(aTunings[0].m_VelrampValue - logf(2.0f) / 1000.0f) < 0.000001f);
  CHECK(aTunings[255].m_Gravity == 0.0f);
  CHECK(GrenadeDoubleExplosion);
  return 0;
}

static int test_world_settings(void) {
  char *apSettings[] = {
      "sv_freeze_delay 99",           "sv_deepfly -2", "sv_teleport_hold_hook 1", "sv_teleport_lose_weapons 1", "sv_destroy_bullets_on_death 0",
      "sv_destroy_lasers_on_death 1", "sv_hit 0",      "sv_endless_drag 1",       "sv_solo_server 1",           "tune player_collision 1",
      "tune_zone 7 player_hooking 1", "switch_open 2", "switch_open 99",
  };
  map_data_t Map = {
      .num_settings = (int)(sizeof(apSettings) / sizeof(apSettings[0])),
      .settings = apSettings,
  };
  SConfig Config;
  init_config(&Config);
  SCollision Collision = {
      .m_MapData = Map,
      .m_HighestSwitchNumber = 3,
  };
  init_test_tunings(Collision.m_aTuningList);
  STeeGrid Grid = tg_empty();
  SWorldCore World = wc_empty();

  wc_init(&World, &Collision, &Grid, &Config);

  CHECK(Config.m_SvFreezeDelay == 30);
  CHECK(Config.m_SvDeepfly == 0);
  CHECK(Config.m_SvTeleportHoldHook == 1);
  CHECK(Config.m_SvTeleportLoseWeapons == 1);
  CHECK(Config.m_SvDestroyBulletsOnDeath == 0);
  CHECK(Config.m_SvDestroyLasersOnDeath == 1);
  CHECK(Config.m_SvHit == 0);
  CHECK(Config.m_SvEndlessDrag == 1);
  CHECK(Config.m_SvSoloServer == 1);
  CHECK(!World.m_pSwitches[2].m_Initial);
  CHECK(!World.m_pSwitches[2].m_Status);
  CHECK(World.m_pSwitches[1].m_Initial);
  for (int Zone = 0; Zone < NUM_TUNE_ZONES; ++Zone) {
    CHECK(Collision.m_aTuningList[Zone].m_PlayerCollision == 0.0f);
    CHECK(Collision.m_aTuningList[Zone].m_PlayerHooking == 0.0f);
  }
  wc_free(&World);
  return 0;
}

int main(void) {
  if (test_tunings() != 0)
    return 1;
  if (test_world_settings() != 0)
    return 1;
  return 0;
}
