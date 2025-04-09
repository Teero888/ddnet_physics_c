#ifndef LIB_GAMECORE_H
#define LIB_GAMECORE_H
#include "collision.h"
#include "stdbool.h"
#include "vmath.h"

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
  char m_Direction;
  int m_TargetX;
  int m_TargetY;
  unsigned char m_Jump;
  unsigned char m_Fire;
  unsigned char m_Hook;
  unsigned char m_WantedWeapon;
} SPlayerInput;

enum { ENTTYPE_PROJECTILE = 0, ENTTYPE_LASER, NUM_ENTTYPES };

bool is_switch_active_cb(int Number, void *pUser);

// Entities {{{

typedef struct Entity {
  struct WorldCore *m_pWorld;
  SCollision *m_pCollision;
  struct Entity *restrict m_pPrevTypeEntity;
  struct Entity *restrict m_pNextTypeEntity;
  vec2 m_Pos;
  int m_ObjType;
  int m_Number;
  int m_Layer;
  bool m_MarkedForDestroy;
} SEntity;

typedef struct Projectile {
  SEntity m_Base;
  vec2 m_Direction;
  STuningParams *m_pTuning;
  int m_LifeSpan;
  int m_Owner;
  int m_Type;
  int m_StartTick;
  int m_Bouncing;
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

  uivec2 m_BlockPos;
  int m_BlockIdx;

  vec2 m_HookPos;
  vec2 m_HookDir;
  vec2 m_HookTeleBase;
  int m_HookTick;
  int m_HookState;

  unsigned char m_ActiveWeapon;
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

  unsigned char m_PrevFire;
  SPlayerInput m_LatestInput;
  SPlayerInput m_Input;
  SPlayerInput m_SavedInput;

  // DDRace
  int m_StartTime;

  unsigned char m_Colliding;
  bool m_LeftWall;
  unsigned char m_TeleCheckpoint;

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
  STuningParams *m_pTuning;
  unsigned char m_MoveRestrictions;
  // we might have more than 255 player ids
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

  // Expects confg/tunings from outside so you can reuse them as many times as
  // needed
  SConfig *m_pConfig;
  STuningParams *m_pTunings;

  SEntity *m_pNextTraverseEntity;
  SEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

  // Store and tick characters seperately from other entities since
  // the amount of players mostly only gets set once for simulations
  // NOTE: i could do this on the stack and just set a max but keep
  // the num characters var for a speedup probably
  int m_NumCharacters;
  SCharacterCore *m_pCharacters;

  int m_NumSwitches;
  SSwitch *m_pSwitches;

  int m_GameTick;

  bool m_NoWeakHook;
  bool m_NoWeakHookAndBounce;
} SWorldCore;

// }}}

void init_config(SConfig *pConfig);
void wc_init(SWorldCore *pCore, SCollision *pCollision, SConfig *pConfig);
void wc_copy_world(SWorldCore *restrict pTo, SWorldCore *restrict pFrom);
void wc_tick(SWorldCore *pCore);
void wc_free(SWorldCore *pCore);

void cc_on_input(SCharacterCore *pCore, const SPlayerInput *pNewInput);
SCharacterCore *wc_add_character(SWorldCore *pWorld);

#endif // LIB_GAMECORE_H
