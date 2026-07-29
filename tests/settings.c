#include "settings.h"

#include <ddnet_physics/gamecore.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
  int m_Amount;
  float m_Angle;
} SDamageIndicatorCapture;

static void count_damage_indicators(mvec2 Pos, float Angle, int Amount, int Cid, void *pUserData) {
  (void)Pos;
  (void)Cid;
  SDamageIndicatorCapture *pCapture = pUserData;
  pCapture->m_Amount += Amount;
  pCapture->m_Angle = Angle;
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

static int test_unique_race_settings(void) {
  char *apSettings[] = {
      "sv_gametype unique",
      "sv_kill_grenades 0",
      "sv_health_and_ammo 1",
      "sv_no_weapons 1",
  };
  map_data_t Map = {
      .num_settings = (int)(sizeof(apSettings) / sizeof(apSettings[0])),
      .settings = apSettings,
  };
  SConfig Config;
  init_config(&Config);
  SCollision Collision = {
      .m_MapData = Map,
  };
  init_test_tunings(Collision.m_aTuningList);
  STeeGrid Grid = tg_empty();
  SWorldCore World = wc_empty();

  wc_init(&World, &Collision, &Grid, &Config);
  CHECK(World.m_UniqueRace);
  CHECK(Config.m_SvKillGrenades == 0);
  CHECK(Config.m_SvHealthAndAmmo == 1);
  CHECK(Config.m_SvNoWeapons == 1);
  CHECK(Config.m_SvDestroyBulletsOnDeath == 0);
  CHECK(Config.m_SvSoloServer == 1);
  wc_free(&World);

  char *apFastcapSettings[] = {
      "sv_gametype fastcap",
      "sv_fastcap 0",
      "sv_health_and_ammo 0",
      "sv_kill_grenades 0",
  };
  Map.num_settings = (int)(sizeof(apFastcapSettings) / sizeof(apFastcapSettings[0]));
  Map.settings = apFastcapSettings;
  Collision.m_MapData = Map;
  init_test_tunings(Collision.m_aTuningList);
  init_config(&Config);
  World = wc_empty();
  wc_init(&World, &Collision, &Grid, &Config);
  CHECK(World.m_UniqueRace);
  CHECK(Config.m_SvFastcap == 1);
  CHECK(Config.m_SvHealthAndAmmo == 1);
  CHECK(Config.m_SvDestroyBulletsOnDeath == 1);
  wc_free(&World);
  return 0;
}

static map_data_t make_unique_race_map(void) {
  enum { WIDTH = 8, HEIGHT = 3 };
  map_data_t Map = {
      .width = WIDTH,
      .height = HEIGHT,
  };
  Map.game_layer.data = calloc(WIDTH * HEIGHT, sizeof(unsigned char));
  Map.game_layer.flags = calloc(WIDTH * HEIGHT, sizeof(unsigned char));
  Map.front_layer.data = calloc(WIDTH * HEIGHT, sizeof(unsigned char));
  Map.front_layer.flags = calloc(WIDTH * HEIGHT, sizeof(unsigned char));
  if (!Map.game_layer.data || !Map.game_layer.flags || !Map.front_layer.data || !Map.front_layer.flags)
    return Map;

  Map.game_layer.data[WIDTH + 1] = ENTITY_OFFSET + ENTITY_SPAWN;
  Map.game_layer.data[WIDTH + 2] = ENTITY_OFFSET + ENTITY_FLAGSTAND_RED;
  Map.game_layer.data[WIDTH + 4] = ENTITY_OFFSET + ENTITY_FLAGSTAND_BLUE;
  Map.game_layer.data[WIDTH + 6] = ENTITY_OFFSET + ENTITY_HEALTH_1;
  Map.front_layer.data[WIDTH + 2] = TILE_START;
  Map.front_layer.data[WIDTH + 4] = TILE_FINISH;
  return Map;
}

static int count_weapon_pickups(const SCollision *pCollision) {
  int Count = 0;
  const int MapSize = pCollision->m_MapData.width * pCollision->m_MapData.height;
  for (int i = 0; i < MapSize; ++i) {
    const SPickup aPickups[] = {pCollision->m_pPickups[i], pCollision->m_pFrontPickups[i]};
    for (size_t Pickup = 0; Pickup < sizeof(aPickups) / sizeof(aPickups[0]); ++Pickup) {
      if (aPickups[Pickup].m_Type == POWERUP_WEAPON || aPickups[Pickup].m_Type == POWERUP_NINJA)
        Count++;
    }
  }
  return Count;
}

static map_data_t make_weapon_pickup_map(void) {
  map_data_t Map = make_unique_race_map();
  if (!Map.game_layer.data)
    return Map;
  const int Offset = Map.width;
  Map.game_layer.data[Offset + 3] = ENTITY_OFFSET + ENTITY_WEAPON_SHOTGUN;
  Map.game_layer.data[Offset + 5] = ENTITY_OFFSET + ENTITY_WEAPON_GRENADE;
  Map.front_layer.data[Offset + 1] = ENTITY_OFFSET + ENTITY_WEAPON_LASER;
  Map.front_layer.data[Offset + 6] = ENTITY_OFFSET + ENTITY_POWERUP_NINJA;
  return Map;
}

static int test_no_weapons_pickups(void) {
  map_data_t Map = make_weapon_pickup_map();
  CHECK(Map.game_layer.data);
  SCollision Collision = {0};
  CHECK(init_collision(&Collision, &Map));
  CHECK(count_weapon_pickups(&Collision) == 4);
  free_collision(&Collision);

  Map = make_weapon_pickup_map();
  CHECK(Map.game_layer.data);
  Collision = (SCollision){0};
  CHECK(init_collision_with_no_weapons(&Collision, &Map, true));
  CHECK(count_weapon_pickups(&Collision) == 0);

  SConfig Config;
  init_config(&Config);
  Config.m_SvFastcap = 1;
  Config.m_SvNoWeapons = 1;
  STeeGrid Grid = tg_empty();
  tg_init(&Grid, Collision.m_MapData.width, Collision.m_MapData.height);
  SWorldCore World = wc_empty();
  wc_init(&World, &Collision, &Grid, &Config);
  SCharacterCore *pCharacter = wc_add_character(&World, 1);
  CHECK(pCharacter);
  CHECK(pCharacter->m_ActiveWeapon == WEAPON_GUN);
  CHECK(!pCharacter->m_aWeaponGot[WEAPON_GRENADE]);
  CHECK(pCharacter->m_aWeaponAmmo[WEAPON_GRENADE] == 0);
  wc_free(&World);
  tg_destroy(&Grid);

  free_collision(&Collision);
  return 0;
}

static int test_game_modes(void) {
  for (EGameMode GameMode = GAME_MODE_DDRACE; GameMode < NUM_GAME_MODES; GameMode++) {
    map_data_t Map = make_weapon_pickup_map();
    CHECK(Map.game_layer.data);

    SCollision Collision = {0};
    SConfig Config;
    STeeGrid Grid = tg_empty();
    SWorldCore World = wc_empty();
    CHECK(init_game_mode(&World, &Collision, &Grid, &Config, &Map, GameMode));

    const bool UniqueRace = GameMode != GAME_MODE_DDRACE;
    const bool Fastcap = GameMode == GAME_MODE_FASTCAP || GameMode == GAME_MODE_FASTCAP_NO_WPNS;
    const bool NoWeapons = GameMode == GAME_MODE_FASTCAP_NO_WPNS;
    CHECK(World.m_UniqueRace == UniqueRace);
    CHECK((Config.m_SvFastcap != 0) == Fastcap);
    CHECK((Config.m_SvNoWeapons != 0) == NoWeapons);
    CHECK(count_weapon_pickups(&Collision) == (NoWeapons ? 0 : 4));

    SCharacterCore *pCharacter = wc_add_character(&World, 1);
    CHECK(pCharacter);
    const bool GrenadeSpawn = GameMode == GAME_MODE_FASTCAP;
    CHECK(pCharacter->m_aWeaponGot[WEAPON_GRENADE] == GrenadeSpawn);
    CHECK(pCharacter->m_ActiveWeapon == (GrenadeSpawn ? WEAPON_GRENADE : WEAPON_GUN));

    wc_free(&World);
    tg_destroy(&Grid);
    free_collision(&Collision);
  }
  return 0;
}

static int test_unique_race_physics(void) {
  map_data_t Map = make_unique_race_map();
  CHECK(Map.game_layer.data && Map.game_layer.flags && Map.front_layer.data && Map.front_layer.flags);

  SCollision Collision = {0};
  CHECK(init_collision(&Collision, &Map));
  CHECK(Collision.m_aFastcapFlagPresent[0]);
  CHECK(Collision.m_aFastcapFlagPresent[1]);

  SConfig Config;
  init_config(&Config);
  Config.m_SvFastcap = 1;
  STeeGrid Grid = tg_empty();
  tg_init(&Grid, Collision.m_MapData.width, Collision.m_MapData.height);
  SWorldCore World = wc_empty();
  wc_init(&World, &Collision, &Grid, &Config);
  SCharacterCore *pCharacter = wc_add_character(&World, 1);
  CHECK(pCharacter);
  CHECK(World.m_UniqueRace);
  CHECK(pCharacter->m_Health == 10);
  CHECK(pCharacter->m_Armor == 10);
  CHECK(pCharacter->m_ActiveWeapon == WEAPON_GRENADE);
  CHECK(pCharacter->m_aWeaponAmmo[WEAPON_GRENADE] == 10);

  pCharacter->m_pTuning->m_Gravity = 0.0f;
  pCharacter->m_pTuning->m_AirFriction = 1.0f;
  pCharacter->m_pTuning->m_VelrampStart = 100000.0f;
  pCharacter->m_Vel = vec2_init(100.0f, 0.0f);
  wc_tick(&World);
  wc_tick(&World);
  CHECK(pCharacter->m_aGotFastcapFlag[0]);
  CHECK(pCharacter->m_aGotFastcapFlag[1]);
  CHECK(pCharacter->m_StartTick == pCharacter->m_FinishTick);
  CHECK(fabsf(pCharacter->m_StartTickOffset) < 0.00001f);
  CHECK(fabsf(pCharacter->m_FinishTickOffset - 0.55f) < 0.00001f);
  CHECK(fabsf(pCharacter->m_RaceTime - 0.011f) < 0.00001f);

  pCharacter->m_Pos = vvadd(Collision.m_aFastcapFlagPositions[0], vec2_init(4.0f * 32.0f, 0.0f));
  pCharacter->m_PrevPos = pCharacter->m_Pos;
  pCharacter->m_Vel = vec2_init(0.0f, 0.0f);
  pCharacter->m_Health = 8;
  cc_calc_indices(pCharacter);
  wc_tick(&World);
  CHECK(pCharacter->m_Health == 9);
  wc_tick(&World);
  CHECK(pCharacter->m_Health == 9);
  CHECK(World.m_pPickupCooldowns && World.m_pPickupCooldowns[0].m_NumEntries == 1);

  SWorldCore Copy = wc_empty();
  wc_init(&Copy, &Collision, &Grid, &Config);
  wc_copy_world(&Copy, &World);
  CHECK(Copy.m_pPickupCooldowns && Copy.m_pPickupCooldowns[0].m_NumEntries == 1);
  CHECK(Copy.m_pPickupCooldowns[0].m_pEntries != World.m_pPickupCooldowns[0].m_pEntries);
  wc_free(&Copy);

  pCharacter->m_Health = 10;
  pCharacter->m_Armor = 10;
  SDamageIndicatorCapture DamageIndicators = {0};
  World.user_data = &DamageIndicators;
  World.damage_indicator = count_damage_indicators;
  CHECK(cc_take_damage(pCharacter, vec2_init(0.0f, 0.0f), 6));
  CHECK(pCharacter->m_Health == 9);
  CHECK(pCharacter->m_Armor == 8);
  CHECK(pCharacter->m_DamageTick == World.m_GameTick);
  CHECK(DamageIndicators.m_Amount == 3);
  CHECK(DamageIndicators.m_Angle == 0.0f);
  CHECK(cc_take_damage(pCharacter, vec2_init(0.0f, 0.0f), 2));
  CHECK(DamageIndicators.m_Amount == 4);
  CHECK(DamageIndicators.m_Angle == 0.25f);

  pCharacter->m_RespawnDelay = 0;
  pCharacter->m_Health = 1;
  pCharacter->m_Armor = 0;
  const uint32_t OldGeneration = pCharacter->m_SpawnGeneration;
  CHECK(!cc_take_damage(pCharacter, vec2_init(0.0f, 0.0f), 2));
  CHECK(pCharacter->m_SpawnGeneration == OldGeneration + 1);
  CHECK(pCharacter->m_Health == 10);

  pCharacter->m_RespawnDelay = 0;
  SPlayerInput Input = {
      .m_TargetX = 1,
      .m_TargetY = 0,
      .m_Fire = 1,
      .m_WantedWeapon = WEAPON_GRENADE,
  };
  cc_on_input(pCharacter, &Input);
  CHECK(World.m_apFirstEntityTypes[WORLD_ENTTYPE_PROJECTILE]);
  CHECK(pCharacter->m_aWeaponAmmo[WEAPON_GRENADE] == 9);
  cc_die(pCharacter);
  wc_tick(&World);
  CHECK(!World.m_apFirstEntityTypes[WORLD_ENTTYPE_PROJECTILE]);

  Config.m_SvFastcap = 0;
  Config.m_SvHealthAndAmmo = 1;
  Config.m_SvKillGrenades = 0;
  Config.m_SvDestroyBulletsOnDeath = 0;
  pCharacter->m_RespawnDelay = 0;
  pCharacter->m_aWeaponGot[WEAPON_GRENADE] = true;
  pCharacter->m_aWeaponAmmo[WEAPON_GRENADE] = 10;
  pCharacter->m_ActiveWeapon = WEAPON_GRENADE;
  pCharacter->m_PrevFire = 0;
  cc_on_input(pCharacter, &Input);
  CHECK(World.m_apFirstEntityTypes[WORLD_ENTTYPE_PROJECTILE]);
  cc_die(pCharacter);
  wc_tick(&World);
  CHECK(World.m_apFirstEntityTypes[WORLD_ENTTYPE_PROJECTILE]);

  wc_free(&World);
  tg_destroy(&Grid);
  free_collision(&Collision);

  Map = make_unique_race_map();
  CHECK(Map.game_layer.data && Map.game_layer.flags && Map.front_layer.data && Map.front_layer.flags);
  Collision = (SCollision){0};
  Grid = tg_empty();
  World = wc_empty();
  CHECK(init_game_mode(&World, &Collision, &Grid, &Config, &Map, GAME_MODE_RACE));
  pCharacter = wc_add_character(&World, 1);
  CHECK(pCharacter);
  pCharacter->m_pTuning->m_Gravity = 0.0f;
  pCharacter->m_pTuning->m_AirFriction = 1.0f;
  pCharacter->m_pTuning->m_VelrampStart = 100000.0f;
  pCharacter->m_Vel = vec2_init(100.0f, 0.0f);
  wc_tick(&World);
  wc_tick(&World);
  CHECK(pCharacter->m_StartTick == pCharacter->m_FinishTick);
  CHECK(fabsf(pCharacter->m_StartTickOffset - 0.16f) < 0.00001f);
  CHECK(fabsf(pCharacter->m_FinishTickOffset - 0.80f) < 0.00001f);
  CHECK(fabsf(pCharacter->m_RaceTime - 0.0128f) < 0.00001f);
  wc_free(&World);
  tg_destroy(&Grid);
  free_collision(&Collision);
  return 0;
}

int main(void) {
  if (test_tunings() != 0)
    return 1;
  if (test_world_settings() != 0)
    return 1;
  if (test_unique_race_settings() != 0)
    return 1;
  if (test_no_weapons_pickups() != 0)
    return 1;
  if (test_game_modes() != 0)
    return 1;
  if (test_unique_race_physics() != 0)
    return 1;
  return 0;
}
