#ifndef LIB_GAMECORE_H
#define LIB_GAMECORE_H
#include "collision.h"
#include "stdbool.h"
#include "vmath.h"

typedef struct TuneParam {
  int m_Value;
} STuneParam;

typedef struct TuningParams {
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description)               \
  STuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
} STuningParams;

typedef struct Config {
#define MACRO_CONFIG_INT(Name, Def) int m_##Name;
#include "config.h"
#undef MACRO_CONFIG_INT
} SConfig;

enum {
  WEAPON_HAMMER = 0,
  WEAPON_GUN,
  WEAPON_SHOTGUN,
  WEAPON_GRENADE,
  WEAPON_LASER,
  WEAPON_NINJA,
  NUM_WEAPONS
};

typedef struct {
  int m_Direction;
  int m_TargetX;
  int m_TargetY;
  int m_Jump;
  int m_Fire;
  int m_Hook;
  int m_WantedWeapon;
} SPlayerInput;

enum { ENTTYPE_PROJECTILE = 0, ENTTYPE_LASER, ENTTYPE_PICKUP, NUM_ENTTYPES };

// Entities {{{

typedef struct Entity {
  struct WorldCore *m_pWorld;
  SCollision *m_pCollision;
  struct Entity *m_pPrevTypeEntity;
  struct Entity *m_pNextTypeEntity;
  vec2 m_Pos;
  float m_ProximityRadius;
  int m_ObjType;
  int m_Number;
  int m_Layer;
  bool m_MarkedForDestroy;
} SEntity;

typedef struct Pickup {
  SEntity m_Base;
  vec2 m_Core;
  int m_Type;
  int m_Subtype;
} SPickup;

typedef struct Projectile {
  SEntity m_Base;
  vec2 m_Direction;
  vec2 m_InitDir;
  int m_LifeSpan;
  int m_Owner;
  int m_Type;
  int m_StartTick;
  int m_Bouncing;
  int m_DDRaceTeam;
  int m_TuneZone;
  bool m_Explosive;
  bool m_Freeze;
  bool m_IsSolo;
} SProjectile;

// }}}

// SCharacter {{{

typedef struct CharacterCore {
  struct WorldCore *m_pWorld;
  SCollision *m_pCollision;
  int m_Id;
  vec2 m_PrevPos;
  vec2 m_Pos;
  vec2 m_Vel;

  vec2 m_HookPos;
  vec2 m_HookDir;
  vec2 m_HookTeleBase;
  int m_HookTick;
  int m_HookState;

  int m_QueuedWeapon;
  int m_ActiveWeapon;
  bool m_aWeaponGot[NUM_WEAPONS];

  // ninja
  struct {
    vec2 m_ActivationDir;
    int m_ActivationTick;
    int m_CurrentMoveTime;
    int m_OldVelAmount;
  } m_Ninja;

  bool m_NewHook;

  int m_Jumped;
  // m_JumpedTotal counts the jumps performed in the air
  int m_JumpedTotal;
  int m_Jumps;

  int m_Direction;

  SPlayerInput m_LatestPrevInput;
  SPlayerInput m_LatestInput;
  SPlayerInput m_Input;
  SPlayerInput m_SavedInput;

  int m_NumInputs;

  // DDRace
  int m_StartTime;

  int m_Colliding;
  bool m_LeftWall;
  int m_TeleCheckpoint;

  // Last refers to the last tick
  bool m_LastRefillJumps;
  bool m_LastPenalty;
  bool m_LastBonus;

  // DDNet Character
  bool m_Solo;
  bool m_Jetpack;
  bool m_CollisionDisabled;
  bool m_EndlessHook;
  bool m_EndlessJump;
  bool m_HammerHitDisabled;
  bool m_GrenadeHitDisabled;
  bool m_LaserHitDisabled;
  bool m_ShotgunHitDisabled;
  bool m_HookHitDisabled;
  bool m_HasTelegunGun;
  bool m_HasTelegunGrenade;
  bool m_HasTelegunLaser;
  int m_FreezeTime;
  bool m_DeepFrozen;
  bool m_LiveFrozen;
  bool m_FrozenLastTick;
  int m_TuneZone;
  STuningParams m_Tuning;

  int m_MoveRestrictions;
  int m_HookedPlayer;

  vec2 m_TeleGunPos;
  bool m_TeleGunTeleport;
  bool m_IsBlueTeleGunTeleport;

  int m_ReloadTimer;

  int m_aHitObjects[10];
  int m_NumObjectsHit;

} SCharacterCore;
// }}}

// World {{{

// We don't want teams for the physics, that makes switches easier
typedef struct {
  bool m_Status;
  bool m_Initial;
  int m_EndTick;
  int m_Type;
  int m_LastUpdateTick;
} SSwitch;

typedef struct WorldCore {
  SCollision *m_pCollision;

  // Expects a config from outside so you can reuse it as many times as needed
  SConfig *m_pConfig;

  SEntity *m_pNextTraverseEntity;
  SEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

  // Store and tick characters seperately from other entities since
  // the amount of players mostly only gets set once for simulations
  // NOTE: i could do this on the stack and just set a max but keep
  // the num characters var for a speedup probably
  int m_NumCharacters;
  SCharacterCore *m_pCharacters;

  int m_NumSwitches;
  SSwitch *m_vSwitches;

  int m_NumTuneZones;
  STuningParams *m_pTuningList;

  int m_GameTick;

  bool m_NoWeakHook;
  bool m_NoWeakHookAndBounce;
} SWorldCore;

// }}}

void init_config(SConfig *pConfig);
void wc_init(SWorldCore *pCore, SCollision *pCollision, SConfig *pConfig);
void wc_tick(SWorldCore *pCore);
void wc_free(SWorldCore *pCore);

void cc_on_input(SCharacterCore *pCore, const SPlayerInput *pNewInput);
SCharacterCore *wc_add_character(SWorldCore *pWorld);

#endif // LIB_GAMECORE_H
