#ifndef LIB_GAMECORE_H
#define LIB_GAMECORE_H
#include "collision.h"
#include "stdbool.h"
#include "vmath.h"

struct TuneParam {
  int m_Value;
} typedef STuneParam;

struct TuningParams {
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description)               \
  STuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
} typedef STuningParams;

enum {
  WEAPON_HAMMER = 0,
  WEAPON_GUN,
  WEAPON_SHOTGUN,
  WEAPON_GRENADE,
  WEAPON_LASER,
  WEAPON_NINJA,
  NUM_WEAPONS
};

struct PlayerInput {
  int m_Direction;
  int m_TargetX;
  int m_TargetY;
  int m_Jump;
  int m_Fire;
  int m_Hook;
  int m_WantedWeapon;
} typedef SPlayerInput;

#define PHYSICALSIZE 28.f
#define PHYSICALSIZEVEC ({vec2}(28.f, 28.f))

struct WorldCore typedef SWorldCore;

enum {
  ENTTYPE_PROJECTILE = 0,
  ENTTYPE_LASER,
  ENTTYPE_PICKUP,
  ENTTYPE_FLAG,
  ENTTYPE_CHARACTER,
  NUM_ENTTYPES
};

// Entities {{{

struct Entity {
  SWorldCore *m_pWorld;
  SCollision *m_pCollision;
  struct Entity *m_pPrevTypeEntity;
  struct Entity *m_pNextTypeEntity;
  vec2 m_Pos;
  float m_ProximityRadius;
  int m_Id;
  int m_ObjType;
  bool m_MarkedForDestroy;
} typedef SEntity;

struct CharacterCore {
  SEntity m_Base;
  vec2 m_Vel;

  vec2 m_HookPos;
  vec2 m_HookDir;
  vec2 m_HookTeleBase;
  int m_HookTick;
  int m_HookState;

  int m_ActiveWeapon;
  struct WeaponStat {
    int m_AmmoRegenStart;
    int m_Ammo;
    int m_Ammocost;
    bool m_Got;
  } m_aWeapons[NUM_WEAPONS];

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
  SPlayerInput m_PrevInput;
  SPlayerInput m_Input;
  SPlayerInput m_SavedInput;
  int m_NumInputs;

  // DDRace
  bool m_Reset;

  int m_Colliding;
  bool m_LeftWall;

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
} typedef SCharacterCore;
// }}}

// World {{{

// We don't want teams for the physics, that makes switches easier
struct Switch {
  bool m_Status;
  bool m_Initial;
  int m_EndTick;
  int m_Type;
  int m_LastUpdateTick;
} typedef SSwitch;

struct WorldCore {
  SCollision *m_pCollision;
  STuningParams *m_pTuningList;

  SEntity *m_pNextTraverseEntity;
  SEntity *m_apFirstEntityTypes[NUM_ENTTYPES];

  int m_NumSwitches;
  SSwitch *m_vSwitches;

  int m_GameTick;

  bool m_NoWeakHook;
  bool m_NoWeakHookAndBounce;
} typedef SWorldCore;

// }}}

#endif // LIB_GAMECORE_H
