#include "gamecore.h"
#include "collision.h"
#include "map_loader.h"
#include "vmath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NINJA_DURATION 15000
#define NINJA_MOVETIME 200
#define NINJA_VELOCITY 50

static void init_tuning_params(STuningParams *pTunings) {
#define MACRO_TUNING_PARAM(Name, Value) pTunings->m_##Name = Value;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
}

void init_config(SConfig *pConfig) {
#define MACRO_CONFIG_INT(Name, Def) pConfig->m_##Name = Def;
#include "config.h"
#undef MACRO_CONFIG_INT
}

// TODO:implement guns, doors, lasers, lights and draggers

// Physics helper functions {{{

vec2 clamp_vel(int MoveRestriction, vec2 Vel) {
  if (Vel.x > 0 && (MoveRestriction & CANTMOVE_RIGHT)) {
    Vel.x = 0;
  } else if (Vel.x < 0 && (MoveRestriction & CANTMOVE_LEFT)) {
    Vel.x = 0;
  }
  if (Vel.y > 0 && (MoveRestriction & CANTMOVE_DOWN)) {
    Vel.y = 0;
  } else if (Vel.y < 0 && (MoveRestriction & CANTMOVE_UP)) {
    Vel.y = 0;
  }
  return Vel;
}

static inline float saturate_add(float Min, float Max, float Current,
                                 float Modifier) {
  if (Modifier < 0) {
    if (Current < Min)
      return Current;
    Current += Modifier;
    if (Current < Min)
      Current = Min;
    return Current;
  } else {
    if (Current > Max)
      return Current;
    Current += Modifier;
    if (Current > Max)
      Current = Max;
    return Current;
  }
}

static inline vec2 calc_pos(vec2 Pos, vec2 Velocity, float Curvature,
                            float Speed, float Time) {
  vec2 n;
  Time *= Speed;
  n.x = Pos.x + Velocity.x * Time;
  n.y = Pos.y + Velocity.y * Time + Curvature / 10000 * (Time * Time);
  return n;
}

static inline float velocity_ramp(float Value, float Start, float Range,
                                  float Curvature) {
  if (Value < Start)
    return 1.0f;
  return 1.0f / pow(Curvature, (Value - Start) / Range);
}

// }}}

// Necessary enums {{{

enum {
  HOOK_RETRACTED = -1,
  HOOK_IDLE = 0,
  HOOK_RETRACT_START = 1,
  HOOK_RETRACT_END = 3,
  HOOK_FLYING,
  HOOK_GRABBED,

  COREEVENT_GROUND_JUMP = 0x01,
  COREEVENT_AIR_JUMP = 0x02,
  COREEVENT_HOOK_LAUNCH = 0x04,
  COREEVENT_HOOK_ATTACH_PLAYER = 0x08,
  COREEVENT_HOOK_ATTACH_GROUND = 0x10,
  COREEVENT_HOOK_HIT_NOHOOK = 0x20,
  COREEVENT_HOOK_RETRACT = 0x40,
};

// }}}

// Fire input logic {{{

#define INPUT_STATE_MASK 0x3f

// input count
typedef struct InputCount {
  int m_Presses;
  int m_Releases;
} SInputCount;

static inline SInputCount count_input(int Prev, int Cur) {
  SInputCount c = {0, 0};
  Prev &= INPUT_STATE_MASK;
  Cur &= INPUT_STATE_MASK;
  int i = Prev;

  while (i != Cur) {
    i = (i + 1) & INPUT_STATE_MASK;
    if (i & 1)
      c.m_Presses++;
    else
      c.m_Releases++;
  }

  return c;
}

// }}}

// Entities {{{

void ent_init(SEntity *pEnt, SWorldCore *pGameWorld, int ObjType, vec2 Pos) {
  pEnt->m_pWorld = pGameWorld;
  pEnt->m_ObjType = ObjType;
  pEnt->m_Pos = Pos;
  pEnt->m_pCollision = pGameWorld->m_pCollision;
  pEnt->m_MarkedForDestroy = false;
  pEnt->m_pPrevTypeEntity = NULL;
  pEnt->m_pNextTypeEntity = NULL;
}

void prj_init(SProjectile *pProj, SWorldCore *pGameWorld, int Type, int Owner,
              vec2 Pos, vec2 Dir, int Span, bool Freeze, bool Explosive,
              vec2 InitDir, int Layer, int Number) {
  memset(pProj, 0, sizeof(SProjectile));
  ent_init(&pProj->m_Base, pGameWorld, ENTTYPE_PROJECTILE, Pos);
  pProj->m_Type = Type;
  pProj->m_Direction = Dir;
  pProj->m_LifeSpan = Span;
  pProj->m_Owner = Owner;
  pProj->m_StartTick = pGameWorld->m_GameTick;
  pProj->m_Explosive = Explosive;
  pProj->m_Base.m_Layer = Layer;
  pProj->m_Base.m_Number = Number;
  pProj->m_Freeze = Freeze;
  pProj->m_InitDir = InitDir;
  pProj->m_TuneZone = is_tune(pGameWorld->m_pCollision,
                              get_map_index(pGameWorld->m_pCollision, Pos));
  pProj->m_IsSolo = pGameWorld->m_pCharacters[Owner].m_Solo;
}

vec2 prj_get_pos(SProjectile *pProj, float Time) {
  float Curvature = 0;
  float Speed = 0;
  STuningParams *pTuning =
      &pProj->m_Base.m_pWorld->m_pTuningList[pProj->m_TuneZone];

  switch (pProj->m_Type) {
  case WEAPON_GRENADE:
    Curvature = pTuning->m_GrenadeCurvature;
    Speed = pTuning->m_GrenadeSpeed;
    break;

  case WEAPON_SHOTGUN:
    Curvature = pTuning->m_ShotgunCurvature;
    Speed = pTuning->m_ShotgunSpeed;
    break;

  case WEAPON_GUN:
    Curvature = pTuning->m_GunCurvature;
    Speed = pTuning->m_GunSpeed;
    break;
  }

  return calc_pos(pProj->m_Base.m_Pos, pProj->m_Direction, Curvature, Speed,
                  Time);
}
SCharacterCore *wc_intersect_character(SWorldCore *pWorld, vec2 Pos0, vec2 Pos1,
                                       float Radius, vec2 *pNewPos,
                                       const SCharacterCore *pNotThis,
                                       const SCharacterCore *pThisOnly);
bool cc_freeze(SCharacterCore *pCore, int Seconds);

void wc_create_explosion(SWorldCore *pWorld, vec2 Pos, int Owner);

void prj_tick(SProjectile *pProj) {
  float Pt = (pProj->m_Base.m_pWorld->m_GameTick - pProj->m_StartTick - 1) /
             (float)SERVER_TICK_SPEED;
  float Ct = (pProj->m_Base.m_pWorld->m_GameTick - pProj->m_StartTick) /
             (float)SERVER_TICK_SPEED;
  vec2 PrevPos = prj_get_pos(pProj, Pt);
  vec2 CurPos = prj_get_pos(pProj, Ct);
  vec2 ColPos;
  vec2 NewPos;
  int Collide = intersect_line(pProj->m_Base.m_pCollision, PrevPos, CurPos,
                               &ColPos, &NewPos);
  SCharacterCore *pOwnerChar = NULL;

  if (pProj->m_Owner >= 0)
    pOwnerChar = &pProj->m_Base.m_pWorld->m_pCharacters[pProj->m_Owner];

  SCharacterCore *pTargetChr = NULL;

  if (pOwnerChar ? !pOwnerChar->m_GrenadeHitDisabled
                 : pProj->m_Base.m_pWorld->m_pConfig->m_SvHit)
    pTargetChr = wc_intersect_character(pProj->m_Base.m_pWorld, PrevPos, ColPos,
                                        6.0f, &ColPos, pOwnerChar, NULL);

  if (pProj->m_LifeSpan > -1)
    pProj->m_LifeSpan--;

  // TODO: the owner can't be "not alive" right now xd
  // so fix this later
  // if (pOwnerChar && pOwnerChar->IsAlive()) {
  //   TeamMask = pOwnerChar->TeamMask();
  // } else if (m_Owner >= 0 &&
  //            (m_Type != WEAPON_GRENADE ||
  //            g_Config.m_SvDestroyBulletsOnDeath)) {
  //   m_MarkedForDestroy = true;
  //   return;
  // }

  if ((pTargetChr &&
       (pOwnerChar ? !pOwnerChar->m_GrenadeHitDisabled
                   : pProj->m_Base.m_pWorld->m_pConfig->m_SvHit ||
                         pProj->m_Owner == -1 || pTargetChr == pOwnerChar)) ||
      Collide) {
    if (pProj->m_Explosive &&
        (!pTargetChr ||
         (pTargetChr && (pProj->m_Type == WEAPON_SHOTGUN && Collide)))) {
      wc_create_explosion(pProj->m_Base.m_pWorld, ColPos, pProj->m_Owner);
    } else if (pProj->m_Freeze) {
      for (int i = 0; i < pProj->m_Base.m_pWorld->m_NumCharacters; ++i) {
        SCharacterCore *pChr = &pProj->m_Base.m_pWorld->m_pCharacters[i];
        if (vdistance(CurPos, pChr->m_Pos) >= 1.f + PHYSICALSIZE)
          continue;
        if (pChr &&
            (pProj->m_Base.m_Layer != LAYER_SWITCH ||
             (pProj->m_Base.m_Layer == LAYER_SWITCH &&
              pProj->m_Base.m_Number > 0 &&
              pProj->m_Base.m_pWorld->m_vSwitches[pProj->m_Base.m_Number]
                  .m_Status)))
          cc_freeze(pChr, pProj->m_Base.m_pWorld->m_pConfig->m_SvFreezeDelay);
      }
    }
    // NOTE: im not sure about this. this might not have any effect
    else if (pTargetChr)
      pTargetChr->m_Vel =
          clamp_vel(pTargetChr->m_MoveRestrictions, pTargetChr->m_Vel);

    if (pOwnerChar &&
        ((pProj->m_Type == WEAPON_GRENADE && pOwnerChar->m_HasTelegunGrenade) ||
         (pProj->m_Type == WEAPON_GUN && pOwnerChar->m_HasTelegunGun))) {
      int MapIndex = get_pure_map_index(
          pProj->m_Base.m_pCollision, pTargetChr ? pTargetChr->m_Pos : ColPos);
      int TileFIndex =
          pProj->m_Base.m_pCollision->m_MapData.m_FrontLayer.m_pData
              ? get_front_tile_index(pProj->m_Base.m_pCollision, MapIndex)
              : 0;
      bool IsSwitchTeleGun = false;
      bool IsBlueSwitchTeleGun = false;
      if (pProj->m_Base.m_pCollision->m_MapData.m_SwitchLayer.m_pType) {
        IsSwitchTeleGun = get_switch_type(pProj->m_Base.m_pCollision,
                                          MapIndex) == TILE_ALLOW_TELE_GUN;
        IsBlueSwitchTeleGun =
            get_switch_type(pProj->m_Base.m_pCollision, MapIndex) ==
            TILE_ALLOW_BLUE_TELE_GUN;
      }

      if (IsSwitchTeleGun || IsBlueSwitchTeleGun) {
        // Delay specifies which weapon the tile should work for.
        // Delay = 0 means all.
        int delay = get_switch_delay(pProj->m_Base.m_pCollision, MapIndex);

        if (delay == 1 && pProj->m_Type != WEAPON_GUN)
          IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
        if (delay == 2 && pProj->m_Type != WEAPON_GRENADE)
          IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
        if (delay == 3 && pProj->m_Type != WEAPON_LASER)
          IsSwitchTeleGun = IsBlueSwitchTeleGun = false;
      }

      if (TileFIndex == TILE_ALLOW_TELE_GUN ||
          TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsSwitchTeleGun ||
          IsBlueSwitchTeleGun || pTargetChr) {
        bool Found;
        vec2 PossiblePos;

        if (!Collide)
          Found = get_nearest_air_pos_player(
              pProj->m_Base.m_pCollision,
              pTargetChr ? pTargetChr->m_Pos : ColPos, &PossiblePos);
        else
          Found = get_nearest_air_pos(pProj->m_Base.m_pCollision, NewPos,
                                      CurPos, &PossiblePos);

        if (Found) {
          pOwnerChar->m_TeleGunPos = PossiblePos;
          pOwnerChar->m_TeleGunTeleport = true;
          pOwnerChar->m_IsBlueTeleGunTeleport =
              TileFIndex == TILE_ALLOW_BLUE_TELE_GUN || IsBlueSwitchTeleGun;
        }
      }
    }

    if (Collide && pProj->m_Bouncing != 0) {
      pProj->m_StartTick = pProj->m_Base.m_pWorld->m_GameTick;
      pProj->m_Base.m_Pos = vvadd(NewPos, vfmul(pProj->m_Direction, -4));
      if (pProj->m_Bouncing == 1)
        pProj->m_Direction.x = -pProj->m_Direction.x;
      else if (pProj->m_Bouncing == 2)
        pProj->m_Direction.y = -pProj->m_Direction.y;
      if (fabs(pProj->m_Direction.x) < 1e-6f)
        pProj->m_Direction.x = 0;
      if (fabs(pProj->m_Direction.y) < 1e-6f)
        pProj->m_Direction.y = 0;
      pProj->m_Base.m_Pos = vvadd(pProj->m_Base.m_Pos, pProj->m_Direction);
    } else if (pProj->m_Type == WEAPON_GUN) {
      pProj->m_Base.m_MarkedForDestroy = true;
      return;
    }
    if (Collide && !pProj->m_Bouncing && !pProj->m_Freeze) {
      pProj->m_Base.m_MarkedForDestroy = true;
      return;
    }
  }
  if (pProj->m_LifeSpan == -1) {
    if (pProj->m_Explosive) {
      wc_create_explosion(pProj->m_Base.m_pWorld, ColPos, pProj->m_Owner);
    }
    pProj->m_Base.m_MarkedForDestroy = true;
    return;
  }

  if (!pProj->m_Base.m_pCollision->m_MapData.m_TeleLayer.m_pType)
    return;
  int x = get_index(pProj->m_Base.m_pCollision, PrevPos, CurPos);
  int z;
  if (pProj->m_Base.m_pWorld->m_pConfig->m_SvOldTeleportWeapons)
    z = is_teleport(pProj->m_Base.m_pCollision, x);
  else
    z = is_teleport_weapon(pProj->m_Base.m_pCollision, x);
  int NumTeleOuts;
  if (z && tele_outs(pProj->m_Base.m_pCollision, z - 1, &NumTeleOuts)) {
    pProj->m_Base.m_Pos = tele_outs(
        pProj->m_Base.m_pCollision, z - 1,
        &NumTeleOuts)[pProj->m_Base.m_pWorld->m_GameTick % NumTeleOuts];
    pProj->m_StartTick = pProj->m_Base.m_pWorld->m_GameTick;
  }
}

enum {
  POWERUP_HEALTH,
  POWERUP_ARMOR,
  POWERUP_WEAPON,
  POWERUP_NINJA,
  POWERUP_ARMOR_SHOTGUN,
  POWERUP_ARMOR_GRENADE,
  POWERUP_ARMOR_NINJA,
  POWERUP_ARMOR_LASER,
  NUM_POWERUPS
};

bool cc_freeze(SCharacterCore *pCore, int Seconds);

void pick_init(SPickup *pPickup, SWorldCore *pGameWorld, int Type, int SubType,
               int Layer, int Number) {
  memset(pPickup, 0, sizeof(SPickup));
  pPickup->m_pWorld = pGameWorld;
  pPickup->m_pCollision = pGameWorld->m_pCollision;
  pPickup->m_Core = vec2_init(0.0f, 0.0f);
  pPickup->m_Type = Type;
  pPickup->m_Subtype = SubType;

  pPickup->m_Layer = Layer;
  pPickup->m_Number = Number;
}

void pick_tick(SPickup *pPickup) {
  if (pPickup->m_pWorld->m_GameTick % (int)(SERVER_TICK_SPEED * 0.15f) == 0) {
    mover_speed(pPickup->m_pCollision, pPickup->m_Pos.x, pPickup->m_Pos.y,
                &pPickup->m_Core);
    pPickup->m_Pos = vvadd(pPickup->m_Pos, pPickup->m_Core);
  }

  for (int i = 0; i < pPickup->m_pWorld->m_NumCharacters; ++i) {
    SCharacterCore *pChar = &pPickup->m_pWorld->m_pCharacters[i];
    vec2 Sub = vvsub(pChar->m_Pos, pPickup->m_Pos);
    // Do cheap check first
    if (vsqlength(Sub) >
            (PICKUPSIZE + 6 + PHYSICALSIZE) * (PICKUPSIZE + 6 + PHYSICALSIZE) ||
        vlength(Sub) >= PICKUPSIZE + 6 + PHYSICALSIZE) {
      continue;
    }

    if (pPickup->m_Layer == LAYER_SWITCH && pPickup->m_Number > 0 &&
        !pPickup->m_pWorld->m_vSwitches[pPickup->m_Number].m_Status)
      continue;

    switch (pPickup->m_Type) {
    case POWERUP_HEALTH:
      // cc_freeze(pChar, pChar->m_pWorld->m_pConfig->m_SvFreezeDelay);
      break;

    case POWERUP_ARMOR:
      for (int j = WEAPON_SHOTGUN; j < NUM_WEAPONS; j++) {
        pChar->m_aWeaponGot[j] = false;
      }
      pChar->m_Ninja.m_ActivationDir = vec2_init(0, 0);
      pChar->m_Ninja.m_ActivationTick = -500;
      pChar->m_Ninja.m_CurrentMoveTime = 0;
      if (pChar->m_ActiveWeapon >= WEAPON_SHOTGUN)
        pChar->m_ActiveWeapon = WEAPON_HAMMER;
      break;

    case POWERUP_ARMOR_SHOTGUN:
      pChar->m_aWeaponGot[WEAPON_SHOTGUN] = false;
      if (pChar->m_ActiveWeapon == WEAPON_SHOTGUN)
        pChar->m_ActiveWeapon = WEAPON_HAMMER;
      break;

    case POWERUP_ARMOR_GRENADE:
      pChar->m_aWeaponGot[WEAPON_GRENADE] = false;
      if (pChar->m_ActiveWeapon == WEAPON_GRENADE)
        pChar->m_ActiveWeapon = WEAPON_HAMMER;
      break;

    case POWERUP_ARMOR_NINJA:
      pChar->m_Ninja.m_ActivationDir = vec2_init(0, 0);
      pChar->m_Ninja.m_ActivationTick = -500;
      pChar->m_Ninja.m_CurrentMoveTime = 0;
      break;

    case POWERUP_ARMOR_LASER:
      pChar->m_aWeaponGot[WEAPON_LASER] = false;
      if (pChar->m_ActiveWeapon == WEAPON_LASER)
        pChar->m_ActiveWeapon = WEAPON_HAMMER;
      break;

    case POWERUP_WEAPON:
      // we do checks for this somewhere else i hope
      pChar->m_aWeaponGot[pPickup->m_Subtype] = true;
      break;

    case POWERUP_NINJA: {
      pChar->m_Ninja.m_ActivationTick = pChar->m_pWorld->m_GameTick;
      pChar->m_aWeaponGot[WEAPON_NINJA] = true;
      pChar->m_ActiveWeapon = WEAPON_NINJA;
      break;
    }
    default:
      break;
    };
  }
}

// }}}

// CharacterCore functions {{{

void cc_reset(SCharacterCore *pCore) {
  memset(pCore, 0, sizeof(SCharacterCore));
  pCore->m_HookedPlayer = -1;
  pCore->m_Jumps = 2;
  pCore->m_Input.m_TargetY = -1;
}

void cc_init(SCharacterCore *pCore, SWorldCore *pWorld) {
  cc_reset(pCore);
  pCore->m_pWorld = pWorld;
  pCore->m_pCollision = pWorld->m_pCollision;
  pCore->m_aWeaponGot[0] = true;
  pCore->m_aWeaponGot[1] = true;

  // The world assigns ids to the core
  pCore->m_Id = -1;
  init_tuning_params(&pCore->m_Tuning);
}

void cc_set_worldcore(SCharacterCore *pCore, SWorldCore *pWorld,
                      SCollision *pCollision) {
  pCore->m_pWorld = pWorld;
  pCore->m_pCollision = pCollision;
}

void cc_unfreeze(SCharacterCore *pCore) {
  if (pCore->m_FreezeTime <= 0)
    return;
  if (!pCore->m_aWeaponGot[pCore->m_ActiveWeapon])
    pCore->m_ActiveWeapon = WEAPON_GUN;
  pCore->m_FreezeTime = 0;
  pCore->m_FrozenLastTick = true;
}

bool is_switch_active_cb(int Number, void *pUser) {
  SCharacterCore *pThis = (SCharacterCore *)pUser;
  return pThis->m_pWorld->m_vSwitches &&
         pThis->m_pWorld->m_vSwitches[Number].m_Status;
}

void cc_quantize(SCharacterCore *pCore) {
  pCore->m_Pos.x = (int)(pCore->m_Pos.x + 0.5f);
  pCore->m_Pos.y = (int)(pCore->m_Pos.y + 0.5f);
  pCore->m_HookPos.x = (int)(pCore->m_HookPos.x + 0.5f);
  pCore->m_HookPos.y = (int)(pCore->m_HookPos.y + 0.5f);

  pCore->m_Vel.x = round_to_int(pCore->m_Vel.x * 256.0f) / 256.0f;
  pCore->m_Vel.y = round_to_int(pCore->m_Vel.y * 256.0f) / 256.0f;
  pCore->m_HookDir.x = round_to_int(pCore->m_HookDir.x * 256.0f) / 256.f;
  pCore->m_HookDir.y = round_to_int(pCore->m_HookDir.y * 256.0f) / 256.f;
}

void cc_move(SCharacterCore *pCore) {
  const float RampValue = velocity_ramp(
      vlength(pCore->m_Vel) * 50, pCore->m_Tuning.m_VelrampStart,
      pCore->m_Tuning.m_VelrampRange, pCore->m_Tuning.m_VelrampCurvature);

  pCore->m_Vel.x *= RampValue;

  vec2 NewPos = pCore->m_Pos;

  vec2 OldVel = pCore->m_Vel;
  bool Grounded = false;
  move_box(pCore->m_pCollision, &NewPos, &pCore->m_Vel,
           vec2_init(pCore->m_Tuning.m_GroundElasticityX,
                     pCore->m_Tuning.m_GroundElasticityY),
           &Grounded);

  if (Grounded) {
    pCore->m_Jumped &= ~2;
    pCore->m_JumpedTotal = 0;
  }

  pCore->m_Colliding = 0;
  if (pCore->m_Vel.x < 0.001f && pCore->m_Vel.x > -0.001f) {
    if (OldVel.x > 0)
      pCore->m_Colliding = 1;
    else if (OldVel.x < 0)
      pCore->m_Colliding = 2;
  } else
    pCore->m_LeftWall = true;

  pCore->m_Vel.x = pCore->m_Vel.x * (1.0f / RampValue);

  if (pCore->m_Tuning.m_PlayerCollision && !pCore->m_CollisionDisabled &&
      !pCore->m_Solo && pCore->m_pWorld->m_NumCharacters > 1) {
    // check player collision
    float Distance = vdistance(pCore->m_Pos, NewPos);
    if (Distance > 0) {
      int End = Distance + 1;
      vec2 LastPos = pCore->m_Pos;
      for (int i = 0; i < End; i++) {
        float a = i / Distance;
        vec2 Pos = vvfmix(pCore->m_Pos, NewPos, a);
        for (int p = 0; p < pCore->m_pWorld->m_NumCharacters; p++) {
          SCharacterCore *pCharCore = &pCore->m_pWorld->m_pCharacters[p];
          if (pCharCore == pCore)
            continue;
          if (pCharCore->m_Solo || pCharCore->m_CollisionDisabled)
            continue;
          float D = vdistance(Pos, pCharCore->m_Pos);
          if (D < PHYSICALSIZE) {
            if (a > 0.0f)
              pCore->m_Pos = LastPos;
            else if (vdistance(NewPos, pCharCore->m_Pos) > D)
              pCore->m_Pos = NewPos;
            return;
          }
        }
        LastPos = Pos;
      }
    }
  }

  pCore->m_Pos = NewPos;
}

void cc_world_tick_deferred(SCharacterCore *pCore) {
  cc_move(pCore);
  cc_quantize(pCore);
}

void cc_tick_deferred(SCharacterCore *pCore) {
  if (pCore->m_pWorld->m_NumCharacters > 1)
    for (int i = 0; i < pCore->m_pWorld->m_NumCharacters; i++) {
      SCharacterCore *pCharCore = &pCore->m_pWorld->m_pCharacters[i];
      if (pCharCore == pCore || pCore->m_Solo || pCharCore->m_Solo)
        continue;

      // handle player <-> player collision
      float Distance = vdistance(pCore->m_Pos, pCharCore->m_Pos);
      if (Distance > 0) {
        vec2 Dir = vnormalize(vvsub(pCore->m_Pos, pCharCore->m_Pos));

        bool CanCollide =
            (!pCore->m_CollisionDisabled && !pCharCore->m_CollisionDisabled &&
             pCore->m_Tuning.m_PlayerCollision);

        if (CanCollide && Distance < PHYSICALSIZE * 1.25f) {
          float a = (PHYSICALSIZE * 1.45f - Distance);
          float Velocity = 0.5f;

          // make sure that we don't add excess force by checking the
          // direction against the current velocity. if not zero.
          if (vlength(pCore->m_Vel) > 0.0001f)
            Velocity = 1 - (vdot(vnormalize(pCore->m_Vel), Dir) + 1) /
                               2; // Wdouble-promotion don't fix this as this
                                  // might change game physics

          pCore->m_Vel = vfmul(
              vvadd(pCore->m_Vel, vfmul(Dir, a * (Velocity * 0.75f))), 0.85f);
        }

        // handle hook influence
        if (!pCore->m_HookHitDisabled && pCore->m_HookedPlayer == i &&
            pCore->m_Tuning.m_PlayerHooking) {
          if (Distance > PHYSICALSIZE * 1.50f) {
            float HookAccel = pCore->m_Tuning.m_HookDragAccel *
                              (Distance / pCore->m_Tuning.m_HookLength);
            float DragSpeed = pCore->m_Tuning.m_HookDragSpeed;

            vec2 Temp;
            // add force to the hooked player
            Temp.x = saturate_add(-DragSpeed, DragSpeed, pCharCore->m_Vel.x,
                                  HookAccel * Dir.x * 1.5f);
            Temp.y = saturate_add(-DragSpeed, DragSpeed, pCharCore->m_Vel.y,
                                  HookAccel * Dir.y * 1.5f);
            pCharCore->m_Vel = clamp_vel(pCharCore->m_MoveRestrictions, Temp);
            // add a little bit force to the guy who has the grip
            Temp.x = saturate_add(-DragSpeed, DragSpeed, pCore->m_Vel.x,
                                  -HookAccel * Dir.x * 0.25f);
            Temp.y = saturate_add(-DragSpeed, DragSpeed, pCore->m_Vel.y,
                                  -HookAccel * Dir.y * 0.25f);
            pCore->m_Vel = clamp_vel(pCore->m_MoveRestrictions, Temp);
          }
        }
      }
    }

  if (pCore->m_HookState != HOOK_FLYING) {
    pCore->m_NewHook = false;
  }

  // clamp the velocity to something sane
  if (vlength(pCore->m_Vel) > 6000)
    pCore->m_Vel = vfmul(vnormalize(pCore->m_Vel), 6000);
}

void cc_ddracetick(SCharacterCore *pCore) {
  memcpy(&pCore->m_Input, &pCore->m_SavedInput, sizeof(pCore->m_Input));

  if (pCore->m_LiveFrozen) {
    pCore->m_Input.m_Direction = 0;
    pCore->m_Input.m_Jump = 0;
  }
  if (pCore->m_FreezeTime > 0) {
    pCore->m_FreezeTime--;
    pCore->m_Input.m_Direction = 0;
    pCore->m_Input.m_Jump = 0;
    pCore->m_Input.m_Hook = 0;
    if (pCore->m_FreezeTime == 1)
      cc_unfreeze(pCore);
  }

  int TuneZoneOld = pCore->m_TuneZone;
  // TODO: implement these functions xd
  int CurrentIndex = get_map_index(pCore->m_pCollision, pCore->m_Pos);
  if (CurrentIndex < 0)
    return;
  pCore->m_TuneZone = is_tune(pCore->m_pCollision, CurrentIndex);
  if (TuneZoneOld != pCore->m_TuneZone)
    pCore->m_Tuning =
        pCore->m_pWorld
            ->m_pTuningList[pCore->m_TuneZone]; // throw tunings from specific
}

void cc_handle_skippable_tiles(SCharacterCore *pCore, int Index) {
  // handle death-tiles and leaving gamelayer
  if ((get_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                        pCore->m_Pos.y - DEATH) == TILE_DEATH ||
       get_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                        pCore->m_Pos.y + DEATH) == TILE_DEATH ||
       get_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                        pCore->m_Pos.y - DEATH) == TILE_DEATH ||
       get_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                        pCore->m_Pos.y + DEATH) == TILE_DEATH ||
       (pCore->m_pCollision->m_MapData.m_FrontLayer.m_pData &&
        (get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                                pCore->m_Pos.y - DEATH) == TILE_DEATH ||
         get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                                pCore->m_Pos.y + DEATH) == TILE_DEATH ||
         get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                                pCore->m_Pos.y - DEATH) == TILE_DEATH ||
         get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                                pCore->m_Pos.y + DEATH) == TILE_DEATH)))) {
    // TODO: implement death logic actually
    // Die(m_pPlayer->GetCid(), WEAPON_WORLD);
    return;
  }

  // NOTE: i don't strictly care about game layer clipping since its basically
  // never used and takes up perf
  // if (GameLayerClipped(pCore->m_Pos)) {
  //   Die(m_pPlayer->GetCid(), WEAPON_WORLD);
  //   return;
  // }

  if (Index < 0)
    return;

  // handle speedup tiles
  if (is_speedup(pCore->m_pCollision, Index)) {
    vec2 Direction, TempVel = pCore->m_Vel;
    int Force, Type, MaxSpeed = 0;
    get_speedup(pCore->m_pCollision, Index, &Direction, &Force, &MaxSpeed,
                &Type);

    if (Type == TILE_SPEED_BOOST_OLD) {
      float TeeAngle, SpeederAngle, DiffAngle, SpeedLeft, TeeSpeed;
      if (Force == 255 && MaxSpeed) {
        pCore->m_Vel = vfmul(Direction, ((float)MaxSpeed / 5.f));
      } else {
        if (MaxSpeed > 0 && MaxSpeed < 5)
          MaxSpeed = 5;
        if (MaxSpeed > 0) {
          if (Direction.x > 0.0000001f)
            SpeederAngle = -atan(Direction.y / Direction.x);
          else if (Direction.x < 0.0000001f)
            SpeederAngle = atan(Direction.y / Direction.x) + 2.0f * asin(1.0f);
          else if (Direction.y > 0.0000001f)
            SpeederAngle = asin(1.0f);
          else
            SpeederAngle = asin(-1.0f);

          if (SpeederAngle < 0)
            SpeederAngle = 4.0f * asin(1.0f) + SpeederAngle;

          if (TempVel.x > 0.0000001f)
            TeeAngle = -atan(TempVel.y / TempVel.x);
          else if (TempVel.x < 0.0000001f)
            TeeAngle = atan(TempVel.y / TempVel.x) + 2.0f * asin(1.0f);
          else if (TempVel.y > 0.0000001f)
            TeeAngle = asin(1.0f);
          else
            TeeAngle = asin(-1.0f);

          if (TeeAngle < 0)
            TeeAngle = 4.0f * asin(1.0f) + TeeAngle;

          TeeSpeed = sqrt(pow(TempVel.x, 2) + pow(TempVel.y, 2));

          DiffAngle = SpeederAngle - TeeAngle;
          SpeedLeft = MaxSpeed / 5.0f - cos(DiffAngle) * TeeSpeed;
          if (abs((int)SpeedLeft) > Force && SpeedLeft > 0.0000001f)
            TempVel = vvadd(TempVel, vfmul(Direction, Force));
          else if (abs((int)SpeedLeft) > Force)
            TempVel = vvadd(TempVel, vfmul(Direction, -Force));
          else
            TempVel = vvadd(TempVel, vfmul(Direction, SpeedLeft));
        } else
          TempVel = vvadd(TempVel, vfmul(Direction, Force));

        pCore->m_Vel = clamp_vel(pCore->m_MoveRestrictions, TempVel);
      }
    } else if (Type == TILE_SPEED_BOOST) {
      static const float MaxSpeedScale = 5.0f;
      if (MaxSpeed == 0) {
        float MaxRampSpeed =
            pCore->m_Tuning.m_VelrampRange /
            (50 * log(fmax((float)pCore->m_Tuning.m_VelrampCurvature, 1.01f)));
        MaxSpeed = fmax(MaxRampSpeed, pCore->m_Tuning.m_VelrampStart) / 50 *
                   MaxSpeedScale;
      }

      float CurrentDirectionalSpeed = vdot(Direction, pCore->m_Vel);
      float TempMaxSpeed = MaxSpeed / MaxSpeedScale;
      if (CurrentDirectionalSpeed + Force > TempMaxSpeed)
        TempVel =
            vvadd(TempVel,
                  vfmul(Direction, (TempMaxSpeed - CurrentDirectionalSpeed)));
      else
        TempVel = vvadd(TempVel, vfmul(Direction, Force));

      pCore->m_Vel = clamp_vel(pCore->m_MoveRestrictions, TempVel);
    }
  }
}

bool cc_freeze(SCharacterCore *pCore, int Seconds) {
  if (Seconds <= 0 || pCore->m_FreezeTime > Seconds * SERVER_TICK_SPEED)
    return false;
  if (pCore->m_FreezeTime == 0) {
    pCore->m_FreezeTime = Seconds * SERVER_TICK_SPEED;
    return true;
  }
  return false;
}

void cc_release_hook(SCharacterCore *pCore) {
  pCore->m_HookedPlayer = -1;
  pCore->m_HookState = HOOK_RETRACTED;
}

void cc_reset_hook(SCharacterCore *pCore) {
  cc_release_hook(pCore);
  pCore->m_HookPos = pCore->m_Pos;
}

void cc_reset_pickups(SCharacterCore *pCore) {
  for (int i = WEAPON_SHOTGUN; i < NUM_WEAPONS - 1; i++) {
    pCore->m_aWeaponGot[i] = false;
    if (pCore->m_ActiveWeapon == i)
      pCore->m_ActiveWeapon = WEAPON_GUN;
  }
}

void wc_release_hooked(SWorldCore *pCore, int Id);
bool wc_next_spawn(SWorldCore *pCore, vec2 *pOutPos);

void cc_handle_tiles(SCharacterCore *pCore, int Index) {
  int MapIndex = Index;

  pCore->m_MoveRestrictions = get_move_restrictions(
      pCore->m_pCollision, is_switch_active_cb, pCore, pCore->m_Pos, MapIndex);
  if (Index < 0) {
    pCore->m_LastRefillJumps = false;
    pCore->m_LastPenalty = false;
    pCore->m_LastBonus = false;
    return;
  }
  int TileIndex = get_tile_index(pCore->m_pCollision, MapIndex);
  int TileFIndex = pCore->m_pCollision->m_MapData.m_FrontLayer.m_pData
                       ? get_front_tile_index(pCore->m_pCollision, MapIndex)
                       : 0;
  if (pCore->m_pCollision->m_MapData.m_TeleLayer.m_pType) {
    int TeleCheckpoint = is_tele_checkpoint(pCore->m_pCollision, MapIndex);
    if (TeleCheckpoint)
      pCore->m_TeleCheckpoint = TeleCheckpoint;
  }

  // freeze
  if ((TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE) &&
      !pCore->m_DeepFrozen) {
    cc_freeze(pCore, pCore->m_pWorld->m_pConfig->m_SvFreezeDelay);
  } else if ((TileIndex == TILE_UNFREEZE || TileFIndex == TILE_UNFREEZE) &&
             !pCore->m_DeepFrozen)
    cc_unfreeze(pCore);

  // deep freeze
  if (TileIndex == TILE_DFREEZE || TileFIndex == TILE_DFREEZE)
    pCore->m_DeepFrozen = true;
  else if (TileIndex == TILE_DUNFREEZE || TileFIndex == TILE_DUNFREEZE)
    pCore->m_DeepFrozen = false;

  // live freeze
  if (TileIndex == TILE_LFREEZE || TileFIndex == TILE_LFREEZE) {
    pCore->m_LiveFrozen = true;
  } else if (TileIndex == TILE_LUNFREEZE || TileFIndex == TILE_LUNFREEZE) {
    pCore->m_LiveFrozen = false;
  }

  // endless hook
  if (TileIndex == TILE_EHOOK_ENABLE || TileFIndex == TILE_EHOOK_ENABLE) {
    pCore->m_EndlessHook = true;
  } else if (TileIndex == TILE_EHOOK_DISABLE ||
             TileFIndex == TILE_EHOOK_DISABLE) {
    pCore->m_EndlessHook = false;
  }

  // hit others
  if (TileIndex == TILE_HIT_DISABLE || TileFIndex == TILE_HIT_DISABLE) {
    pCore->m_HammerHitDisabled = true;
    pCore->m_ShotgunHitDisabled = true;
    pCore->m_GrenadeHitDisabled = true;
    pCore->m_LaserHitDisabled = true;
  } else if (TileIndex == TILE_HIT_ENABLE || TileFIndex == TILE_HIT_ENABLE) {
    pCore->m_ShotgunHitDisabled = false;
    pCore->m_GrenadeHitDisabled = false;
    pCore->m_HammerHitDisabled = false;
    pCore->m_LaserHitDisabled = false;
  }

  // collide with others
  if (TileIndex == TILE_NPC_DISABLE || TileFIndex == TILE_NPC_DISABLE) {
    pCore->m_CollisionDisabled = true;
  } else if (TileIndex == TILE_NPC_ENABLE || TileFIndex == TILE_NPC_ENABLE) {
    pCore->m_CollisionDisabled = false;
  }

  // hook others
  if ((TileIndex == TILE_NPH_DISABLE) || (TileFIndex == TILE_NPH_DISABLE)) {
    pCore->m_HookHitDisabled = true;
  } else if (TileIndex == TILE_NPH_ENABLE || TileFIndex == TILE_NPH_ENABLE) {
    pCore->m_HookHitDisabled = false;
  }

  // unlimited air jumps
  if (TileIndex == TILE_UNLIMITED_JUMPS_ENABLE ||
      TileFIndex == TILE_UNLIMITED_JUMPS_ENABLE) {
    pCore->m_EndlessJump = true;
  } else if (TileIndex == TILE_UNLIMITED_JUMPS_DISABLE ||
             TileFIndex == TILE_UNLIMITED_JUMPS_DISABLE) {
    pCore->m_EndlessJump = false;
  }

  // walljump
  if (TileIndex == TILE_WALLJUMP || TileFIndex == TILE_WALLJUMP) {
    if (pCore->m_Vel.y > 0 && pCore->m_Colliding && pCore->m_LeftWall) {
      pCore->m_LeftWall = false;
      pCore->m_JumpedTotal = pCore->m_Jumps >= 2 ? pCore->m_Jumps - 2 : 0;
      pCore->m_Jumped = 1;
    }
  }

  // jetpack gun
  if (TileIndex == TILE_JETPACK_ENABLE || TileFIndex == TILE_JETPACK_ENABLE) {
    pCore->m_Jetpack = true;
  } else if (TileIndex == TILE_JETPACK_DISABLE ||
             TileFIndex == TILE_JETPACK_DISABLE) {
    pCore->m_Jetpack = false;
  }

  // refill jumps
  if ((TileIndex == TILE_REFILL_JUMPS || TileFIndex == TILE_REFILL_JUMPS) &&
      !pCore->m_LastRefillJumps) {
    pCore->m_JumpedTotal = 0;
    pCore->m_Jumped = 0;
    pCore->m_LastRefillJumps = true;
  }
  if (TileIndex != TILE_REFILL_JUMPS && TileFIndex != TILE_REFILL_JUMPS) {
    pCore->m_LastRefillJumps = false;
  }

  // Teleport gun
  if (TileIndex == TILE_TELE_GUN_ENABLE || TileFIndex == TILE_TELE_GUN_ENABLE) {
    pCore->m_HasTelegunGun = true;
  } else if (TileIndex == TILE_TELE_GUN_DISABLE ||
             TileFIndex == TILE_TELE_GUN_DISABLE) {
    pCore->m_HasTelegunGun = false;
  }

  if (TileIndex == TILE_TELE_GRENADE_ENABLE ||
      TileFIndex == TILE_TELE_GRENADE_ENABLE) {
    pCore->m_HasTelegunGrenade = true;
  } else if (TileIndex == TILE_TELE_GRENADE_DISABLE ||
             TileFIndex == TILE_TELE_GRENADE_DISABLE) {
    pCore->m_HasTelegunGrenade = false;
  }

  if (((TileIndex == TILE_TELE_LASER_ENABLE) ||
       (TileFIndex == TILE_TELE_LASER_ENABLE)) &&
      !pCore->m_HasTelegunLaser) {
    pCore->m_HasTelegunLaser = true;
  } else if (TileIndex == TILE_TELE_LASER_DISABLE ||
             TileFIndex == TILE_TELE_LASER_DISABLE) {
    pCore->m_HasTelegunLaser = false;
  }

  // stopper
  if (pCore->m_Vel.y > 0 && (pCore->m_MoveRestrictions & CANTMOVE_DOWN)) {
    pCore->m_Jumped = 0;
    pCore->m_JumpedTotal = 0;
  }
  // apply move restrictions
  pCore->m_Vel = clamp_vel(pCore->m_MoveRestrictions, pCore->m_Vel);

  SSwitch *pSwitches = pCore->m_pWorld->m_vSwitches;
  if (pSwitches) {
    unsigned char Number = get_switch_number(pCore->m_pCollision, MapIndex);
    unsigned char Type = get_switch_type(pCore->m_pCollision, MapIndex);
    unsigned char Delay = get_switch_delay(pCore->m_pCollision, MapIndex);
    int Tick = pCore->m_pWorld->m_GameTick;

    SSwitch *pSwitch = &pSwitches[Number];

    // handle switch tiles
    if (Type == TILE_SWITCHOPEN && Number > 0) {
      pSwitch->m_Status = true;
      pSwitch->m_EndTick = 0;
      pSwitch->m_Type = TILE_SWITCHOPEN;
      pSwitch->m_LastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHTIMEDOPEN && Number > 0) {
      pSwitch->m_Status = true;
      pSwitch->m_EndTick = Tick + 1 + Delay * SERVER_TICK_SPEED;
      pSwitch->m_Type = TILE_SWITCHTIMEDOPEN;
      pSwitch->m_LastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHTIMEDCLOSE && Number > 0) {
      pSwitch->m_Status = false;
      pSwitch->m_EndTick = Tick + 1 + Delay * SERVER_TICK_SPEED;
      pSwitch->m_Type = TILE_SWITCHTIMEDCLOSE;
      pSwitch->m_LastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHCLOSE && Number > 0) {
      pSwitch->m_Status = false;
      pSwitch->m_EndTick = 0;
      pSwitch->m_Type = TILE_SWITCHCLOSE;
      pSwitch->m_LastUpdateTick = Tick;
    } else if (Type == TILE_FREEZE) {
      if (Number == 0 || pSwitch->m_Status) {
        cc_freeze(pCore, Delay);
      }
    } else if (Type == TILE_DFREEZE) {
      if (Number == 0 || pSwitch->m_Status)
        pCore->m_DeepFrozen = true;
    } else if (Type == TILE_DUNFREEZE) {
      if (Number == 0 || pSwitch->m_Status)
        pCore->m_DeepFrozen = false;
    } else if (Type == TILE_LFREEZE) {
      if (Number == 0 || pSwitch->m_Status) {
        pCore->m_LiveFrozen = true;
      }
    } else if (Type == TILE_LUNFREEZE) {
      if (Number == 0 || pSwitch->m_Status) {
        pCore->m_LiveFrozen = false;
      }
    } else if (Type == TILE_HIT_ENABLE && Delay == WEAPON_HAMMER) {
      pCore->m_HammerHitDisabled = false;
    } else if (Type == TILE_HIT_DISABLE && Delay == WEAPON_HAMMER) {
      pCore->m_HammerHitDisabled = true;
    } else if (Type == TILE_HIT_ENABLE && Delay == WEAPON_SHOTGUN) {
      pCore->m_ShotgunHitDisabled = false;
    } else if (Type == TILE_HIT_DISABLE && Delay == WEAPON_SHOTGUN) {
      pCore->m_ShotgunHitDisabled = true;
    } else if (Type == TILE_HIT_ENABLE && Delay == WEAPON_GRENADE) {
      pCore->m_GrenadeHitDisabled = false;
    } else if (Type == TILE_HIT_DISABLE && Delay == WEAPON_GRENADE) {
      pCore->m_GrenadeHitDisabled = true;
    } else if (Type == TILE_HIT_ENABLE && Delay == WEAPON_LASER) {
      pCore->m_LaserHitDisabled = false;
    } else if (Type == TILE_HIT_DISABLE && Delay == WEAPON_LASER) {
      pCore->m_LaserHitDisabled = true;
    } else if (Type == TILE_JUMP) {
      int NewJumps = Delay;
      if (NewJumps == 255) {
        NewJumps = -1;
      }
      if (NewJumps != pCore->m_Jumps) {
        pCore->m_Jumps = NewJumps;
      }
    } else if (Type == TILE_ADD_TIME && !pCore->m_LastPenalty) {
      int min = Delay;
      int sec = Number;
      pCore->m_StartTime -= (min * 60 + sec) * SERVER_TICK_SPEED;
      pCore->m_LastPenalty = true;
    } else if (Type == TILE_SUBTRACT_TIME && !pCore->m_LastBonus) {
      int min = Delay;
      int sec = Number;
      pCore->m_StartTime += (min * 60 + sec) * SERVER_TICK_SPEED;
      if (pCore->m_StartTime > Tick)
        pCore->m_StartTime = Tick;
      pCore->m_LastBonus = true;
    }
    if (Type != TILE_ADD_TIME) {
      pCore->m_LastPenalty = false;
    }
    if (Type != TILE_SUBTRACT_TIME) {
      pCore->m_LastBonus = false;
    }
  }

  if (!pCore->m_pCollision->m_MapData.m_TeleLayer.m_pType)
    return;

  int z = is_teleport(pCore->m_pCollision, MapIndex);
  int Num;
  SConfig *pConfig = pCore->m_pWorld->m_pConfig;
  if (z && tele_outs(pCore->m_pCollision, z - 1, &Num) && Num > 0 &&
      !pConfig->m_SvOldTeleportHook && !pConfig->m_SvOldTeleportWeapons) {

    // TODO: make this be controlled by player input later
    pCore->m_Pos = tele_outs(pCore->m_pCollision, z - 1,
                             &Num)[pCore->m_pWorld->m_GameTick % Num];
    if (!pConfig->m_SvTeleportHoldHook) {
      cc_reset_hook(pCore);
    }
    if (pConfig->m_SvTeleportLoseWeapons)
      cc_reset_pickups(pCore);
    return;
  }
  int evilz = is_evil_teleport(pCore->m_pCollision, MapIndex);
  if (evilz && tele_outs(pCore->m_pCollision, evilz, &Num) && Num > 0) {
    // TODO: make this be controlled by player input later
    pCore->m_Pos = tele_outs(pCore->m_pCollision, evilz - 1,
                             &Num)[pCore->m_pWorld->m_GameTick % Num];
    if (!pConfig->m_SvOldTeleportHook && !pConfig->m_SvOldTeleportWeapons) {
      pCore->m_Vel = vec2_init(0, 0);

      if (!pConfig->m_SvTeleportHoldHook) {
        cc_reset_hook(pCore);
        wc_release_hooked(pCore->m_pWorld, pCore->m_Id);
      }
      if (pConfig->m_SvTeleportLoseWeapons) {
        cc_reset_pickups(pCore);
      }
    }
    return;
  }
  if (is_check_evil_teleport(pCore->m_pCollision, MapIndex)) {
    // first check if there is a TeleCheckOut for the current recorded
    // checkpoint, if not check previous checkpoints
    for (int k = pCore->m_TeleCheckpoint - 1; k >= 0; k--) {
      if (tele_check_outs(pCore->m_pCollision, k, &Num)) {
        pCore->m_Pos = tele_check_outs(pCore->m_pCollision, k,
                                       &Num)[pCore->m_pWorld->m_GameTick % Num];
        pCore->m_Vel = vec2_init(0, 0);

        if (!pConfig->m_SvTeleportHoldHook) {
          cc_reset_hook(pCore);
          wc_release_hooked(pCore->m_pWorld, pCore->m_Id);
        }

        return;
      }
    }
    // if no checkpointout have been found (or if there no recorded
    // checkpoint), teleport to start
    vec2 SpawnPos;
    if (wc_next_spawn(pCore->m_pWorld, &SpawnPos)) {
      pCore->m_Pos = SpawnPos;
      pCore->m_Vel = vec2_init(0, 0);

      if (!pConfig->m_SvTeleportHoldHook) {
        cc_reset_hook(pCore);
        wc_release_hooked(pCore->m_pWorld, pCore->m_Id);
      }
    }
    return;
  }

  if (is_check_teleport(pCore->m_pCollision, MapIndex)) {
    // first check if there is a TeleCheckOut for the current recorded
    // checkpoint, if not check previous checkpoints
    for (int k = pCore->m_TeleCheckpoint - 1; k >= 0; k--) {
      if (tele_check_outs(pCore->m_pCollision, k, &Num)) {
        pCore->m_Pos = tele_check_outs(pCore->m_pCollision, k,
                                       &Num)[pCore->m_pWorld->m_GameTick % Num];

        if (!pConfig->m_SvTeleportHoldHook) {
          cc_reset_hook(pCore);
        }

        return;
      }
    }
    // if no checkpointout have been found (or if there no recorded
    // checkpoint), teleport to start
    vec2 SpawnPos;
    if (wc_next_spawn(pCore->m_pWorld, &SpawnPos)) {
      pCore->m_Pos = SpawnPos;

      if (!pConfig->m_SvTeleportHoldHook) {
        cc_reset_hook(pCore);
      }
    }
    return;
  }
}

void cc_ddrace_postcore_tick(SCharacterCore *pCore) {

  if (pCore->m_EndlessHook)
    pCore->m_HookTick = 0;

  pCore->m_FrozenLastTick = false;

  if (pCore->m_DeepFrozen)
    cc_freeze(pCore, pCore->m_pWorld->m_pConfig->m_SvFreezeDelay);

  // following jump rules can be overridden by tiles, like Refill Jumps,
  // Stopper and Wall Jump
  if (pCore->m_Jumps == -1) {
    // The player has only one ground jump, so his feet are always dark
    pCore->m_Jumped |= 2;
  } else if (pCore->m_Jumps == 0) {
    // The player has no jumps at all, so his feet are always dark
    pCore->m_Jumped |= 2;
  } else if (pCore->m_Jumps == 1 && pCore->m_Jumped > 0) {
    // If the player has only one jump, each jump is the last one
    pCore->m_Jumped |= 2;
  } else if (pCore->m_JumpedTotal < pCore->m_Jumps - 1 && pCore->m_Jumped > 1) {
    // The player has not yet used up all his jumps, so his feet remain light
    pCore->m_Jumped = 1;
  }

  if ((pCore->m_EndlessJump) && pCore->m_Jumped > 1) {
    // Super players and players with infinite jumps always have light feet
    pCore->m_Jumped = 1;
  }

  int CurrentIndex = get_map_index(pCore->m_pCollision, pCore->m_Pos);
  cc_handle_skippable_tiles(pCore, CurrentIndex);

  float d = vdistance(pCore->m_PrevPos, pCore->m_Pos);
  int End = d + 1;
  bool Handled = false;
  if (!d) {
    int Nx = (int)pCore->m_Pos.x >> 5;
    int Ny = (int)pCore->m_Pos.y >> 5;
    int Index = Ny * pCore->m_pCollision->m_MapData.m_Width + Nx;

    if (pCore->m_pCollision->m_pTileInfos[Index] & INFO_TILENEXT) {
      Handled = true;
      cc_handle_tiles(pCore, Index);
    }
  } else {
    int LastIndex = 0;
    for (int i = 0; i < End; i++) {
      float a = i / d;
      vec2 Tmp = vvfmix(pCore->m_PrevPos, pCore->m_Pos, a);
      int Nx = (int)Tmp.x >> 5;
      int Ny = (int)Tmp.y >> 5;
      int Index = Ny * pCore->m_pCollision->m_MapData.m_Width + Nx;
      if (pCore->m_pCollision->m_pTileInfos[Index] & INFO_TILENEXT &&
          LastIndex != Index) {
        cc_handle_tiles(pCore, Index);
        LastIndex = Index;
        Handled = true;
      }
    }
  }
  if (!Handled) {
    cc_handle_tiles(pCore, CurrentIndex);
  }

  // teleport gun
  if (pCore->m_TeleGunTeleport) {
    pCore->m_Pos = pCore->m_TeleGunPos;
    if (!pCore->m_IsBlueTeleGunTeleport)
      pCore->m_Vel = vec2_init(0, 0);
    pCore->m_TeleGunTeleport = false;
    pCore->m_IsBlueTeleGunTeleport = false;
  }
}

void cc_pre_tick(SCharacterCore *pCore) {
  cc_ddracetick(pCore);

  pCore->m_MoveRestrictions = get_move_restrictions(
      pCore->m_pCollision, is_switch_active_cb, pCore, pCore->m_Pos, -1);

  // get ground state
  const bool Grounded =
      check_point(pCore->m_pCollision,
                  vec2_init(pCore->m_Pos.x + PHYSICALSIZE / 2,
                            pCore->m_Pos.y + PHYSICALSIZE / 2 + 5)) ||
      check_point(pCore->m_pCollision,
                  vec2_init(pCore->m_Pos.x - PHYSICALSIZE / 2,
                            pCore->m_Pos.y + PHYSICALSIZE / 2 + 5));

  vec2 TargetDirection =
      vnormalize(vec2_init(pCore->m_Input.m_TargetX, pCore->m_Input.m_TargetY));

  pCore->m_Vel.y += pCore->m_Tuning.m_Gravity;

  float MaxSpeed = Grounded ? pCore->m_Tuning.m_GroundControlSpeed
                            : pCore->m_Tuning.m_AirControlSpeed;
  float Accel = Grounded ? pCore->m_Tuning.m_GroundControlAccel
                         : pCore->m_Tuning.m_AirControlAccel;
  float Friction = Grounded ? pCore->m_Tuning.m_GroundFriction
                            : pCore->m_Tuning.m_AirFriction;

  // handle input
  pCore->m_Direction = pCore->m_Input.m_Direction;

  // Special jump cases:
  // m_Jumps == -1: A tee may only make one ground jump. Second jumped bit is
  // always set m_Jumps == 0: A tee may not make a jump. Second jumped bit is
  // always set m_Jumps == 1: A tee may do either a ground jump or an air
  // jump. Second jumped bit is set after the first jump The second jumped bit
  // can be overridden by special tiles so that the tee can nevertheless jump.

  // handle jump
  if (pCore->m_Input.m_Jump) {
    if (!(pCore->m_Jumped & 1)) {
      if (Grounded && (!(pCore->m_Jumped & 2) || pCore->m_Jumps != 0)) {
        pCore->m_Vel.y = -pCore->m_Tuning.m_GroundJumpImpulse;
        if (pCore->m_Jumps > 1) {
          pCore->m_Jumped |= 1;
        } else {
          pCore->m_Jumped |= 3;
        }
        pCore->m_JumpedTotal = 0;
      } else if (!(pCore->m_Jumped & 2)) {
        pCore->m_Vel.y = -pCore->m_Tuning.m_AirJumpImpulse;
        pCore->m_Jumped |= 3;
        pCore->m_JumpedTotal++;
      }
    }
  } else {
    pCore->m_Jumped &= ~1;
  }

  // handle hook
  if (pCore->m_Input.m_Hook) {
    if (pCore->m_HookState == HOOK_IDLE) {
      pCore->m_HookState = HOOK_FLYING;
      pCore->m_HookPos =
          vvadd(pCore->m_Pos, vfmul(TargetDirection, PHYSICALSIZE * 1.5f));
      pCore->m_HookDir = TargetDirection;
      pCore->m_HookedPlayer = -1;
      pCore->m_HookTick =
          (float)SERVER_TICK_SPEED * (1.25f - pCore->m_Tuning.m_HookDuration);
    }
  } else {
    pCore->m_HookedPlayer = -1;
    pCore->m_HookState = HOOK_IDLE;
    pCore->m_HookPos = pCore->m_Pos;
  }

  // handle jumping
  // 1 bit = to keep track if a jump has been made on this input (player is
  // holding space bar) 2 bit = to track if all air-jumps have been used up
  // (tee gets dark feet)
  if (Grounded) {
    pCore->m_Jumped &= ~2;
    pCore->m_JumpedTotal = 0;
  }

  // add the speed modification according to players wanted direction
  if (pCore->m_Direction < 0)
    pCore->m_Vel.x = saturate_add(-MaxSpeed, MaxSpeed, pCore->m_Vel.x, -Accel);
  if (pCore->m_Direction > 0)
    pCore->m_Vel.x = saturate_add(-MaxSpeed, MaxSpeed, pCore->m_Vel.x, Accel);
  if (pCore->m_Direction == 0)
    pCore->m_Vel.x *= Friction;

  // do hook
  if (pCore->m_HookState == HOOK_IDLE) {
    pCore->m_HookedPlayer = -1;
    pCore->m_HookPos = pCore->m_Pos;
  } else if (pCore->m_HookState >= HOOK_RETRACT_START &&
             pCore->m_HookState < HOOK_RETRACT_END) {
    pCore->m_HookState++;
  } else if (pCore->m_HookState == HOOK_RETRACT_END) {
    pCore->m_HookState = HOOK_RETRACTED;
  } else if (pCore->m_HookState == HOOK_FLYING) {
    vec2 HookBase = pCore->m_Pos;
    if (pCore->m_NewHook) {
      HookBase = pCore->m_HookTeleBase;
    }
    vec2 NewPos =
        vvadd(pCore->m_HookPos,
              vfmul(pCore->m_HookDir, pCore->m_Tuning.m_HookFireSpeed));
    if (vdistance(HookBase, NewPos) > pCore->m_Tuning.m_HookLength) {
      pCore->m_HookState = HOOK_RETRACT_START;
      NewPos = vvadd(HookBase, vfmul(vnormalize(vvsub(NewPos, HookBase)),
                                     pCore->m_Tuning.m_HookLength));
    }

    // make sure that the hook doesn't go though the ground
    bool GoingToHitGround = false;
    bool GoingToRetract = false;
    bool GoingThroughTele = false;
    int teleNr = 0;
    int Hit = intersect_line_tele_hook(
        pCore->m_pCollision, pCore->m_HookPos, NewPos, &NewPos,
        pCore->m_pCollision->m_MapData.m_TeleLayer.m_pType ? &teleNr : NULL,
        pCore->m_pWorld->m_pConfig->m_SvOldTeleportHook);

    if (Hit) {
      if (Hit == TILE_NOHOOK)
        GoingToRetract = true;
      else if (Hit == TILE_TELEINHOOK)
        GoingThroughTele = true;
      else
        GoingToHitGround = true;
    }

    // Check against other players first
    if (!pCore->m_HookHitDisabled && pCore->m_pWorld &&
        pCore->m_Tuning.m_PlayerHooking &&
        (pCore->m_HookState == HOOK_FLYING || !pCore->m_NewHook)) {
      float Distance = 0.0f;

      for (int i = 0; i < pCore->m_pWorld->m_NumCharacters; ++i) {
        SCharacterCore *pEntity = &pCore->m_pWorld->m_pCharacters[i];
        if (pEntity == pCore || pEntity->m_Solo || pCore->m_Solo)
          continue;

        vec2 ClosestPoint;
        if (closest_point_on_line(pCore->m_HookPos, NewPos, pEntity->m_Pos,
                                  &ClosestPoint)) {
          if (vdistance(pEntity->m_Pos, ClosestPoint) < PHYSICALSIZE + 2.0f) {
            if (pCore->m_HookedPlayer == -1 ||
                vdistance(pCore->m_HookPos, pEntity->m_Pos) < Distance) {
              pCore->m_HookState = HOOK_GRABBED;
              pCore->m_HookedPlayer = -1;
              Distance = vdistance(pCore->m_HookPos, pEntity->m_Pos);
            }
          }
        }
      }
    }

    if (pCore->m_HookState == HOOK_FLYING) {
      // check against ground
      if (GoingToHitGround) {
        pCore->m_HookState = HOOK_GRABBED;
      } else if (GoingToRetract) {
        pCore->m_HookState = HOOK_RETRACT_START;
      }
      int NumOuts;
      const vec2 *pTeleOuts =
          tele_outs(pCore->m_pCollision, teleNr - 1, &NumOuts);
      if (GoingThroughTele && NumOuts > 0) {
        pCore->m_HookedPlayer = -1;

        pCore->m_NewHook = true;
        // TODO: add a proper system for this.
        // i don't want to use random number obviously since this is for
        // simulation purposes so the player should be able to control this
        // with an input
        pCore->m_HookPos =
            vvadd(pTeleOuts[pCore->m_pWorld->m_GameTick % NumOuts],
                  vfmul(TargetDirection, PHYSICALSIZE * 1.5f));
        pCore->m_HookDir = TargetDirection;
        pCore->m_HookTeleBase = pCore->m_HookPos;
      } else {
        pCore->m_HookPos = NewPos;
      }
    }
  }

  if (pCore->m_HookState == HOOK_GRABBED) {
    if (pCore->m_HookedPlayer != -1 && pCore->m_pWorld) {
      SCharacterCore *pCharCore =
          &pCore->m_pWorld->m_pCharacters[pCore->m_HookedPlayer];
      if (pCharCore && pCore->m_Id != -1)
        pCore->m_HookPos = pCharCore->m_Pos;
      else {
        // release hook
        pCore->m_HookedPlayer = -1;
        pCore->m_HookState = HOOK_RETRACTED;
        pCore->m_HookPos = pCore->m_Pos;
      }
    }

    // don't do this hook routine when we are already hooked to a player
    if (pCore->m_HookedPlayer == -1 &&
        vdistance(pCore->m_HookPos, pCore->m_Pos) > 46.0f) {
      vec2 HookVel = vfmul(vnormalize(vvsub(pCore->m_HookPos, pCore->m_Pos)),
                           pCore->m_Tuning.m_HookDragAccel);
      // the hook as more power to drag you up then down.
      // this makes it easier to get on top of an platform
      if (HookVel.y > 0)
        HookVel.y *= 0.3f;

      // the hook will boost it's power if the player wants to move
      // in that direction. otherwise it will dampen everything abit
      if ((HookVel.x < 0 && pCore->m_Direction < 0) ||
          (HookVel.x > 0 && pCore->m_Direction > 0))
        HookVel.x *= 0.95f;
      else
        HookVel.x *= 0.75f;

      vec2 NewVel = vvadd(pCore->m_Vel, HookVel);

      // check if we are under the legal limit for the hook
      const float NewVelLength = vlength(NewVel);
      if (NewVelLength < pCore->m_Tuning.m_HookDragSpeed ||
          NewVelLength < vlength(pCore->m_Vel))
        pCore->m_Vel = NewVel; // no problem. apply
    }

    // release hook (max default hook time is 1.25 s)
    pCore->m_HookTick++;
    if (pCore->m_HookedPlayer != -1 &&
        (pCore->m_HookTick > SERVER_TICK_SPEED + SERVER_TICK_SPEED / 5 ||
         (pCore->m_HookedPlayer >= pCore->m_pWorld->m_NumCharacters))) {
      pCore->m_HookedPlayer = -1;
      pCore->m_HookState = HOOK_RETRACTED;
      pCore->m_HookPos = pCore->m_Pos;
    }
  }

  if (!pCore->m_pWorld->m_NoWeakHookAndBounce)
    cc_tick_deferred(pCore);
}

void cc_set_weapon(SCharacterCore *pCore, int W) {

  if (W == pCore->m_ActiveWeapon)
    return;

  pCore->m_QueuedWeapon = -1;
  pCore->m_ActiveWeapon = W;

  if (pCore->m_ActiveWeapon < 0 || pCore->m_ActiveWeapon >= NUM_WEAPONS)
    pCore->m_ActiveWeapon = 0;
}

void cc_remove_ninja(SCharacterCore *pCore) {
  pCore->m_Ninja.m_ActivationDir = vec2_init(0, 0);
  pCore->m_Ninja.m_ActivationTick = 0;
  pCore->m_Ninja.m_CurrentMoveTime = 0;
  pCore->m_Ninja.m_OldVelAmount = 0;
  pCore->m_aWeaponGot[WEAPON_NINJA] = false;

  cc_set_weapon(pCore, pCore->m_ActiveWeapon);
}

void cc_take_damage(SCharacterCore *pCore, vec2 Force) {
  pCore->m_Vel =
      clamp_vel(pCore->m_MoveRestrictions, vvadd(pCore->m_Vel, Force));
}

void cc_handle_ninja(SCharacterCore *pCore) {

  if ((pCore->m_pWorld->m_GameTick - pCore->m_Ninja.m_ActivationTick) >
      (NINJA_DURATION * SERVER_TICK_SPEED / 1000)) {
    // time's up, return
    cc_remove_ninja(pCore);
    return;
  }

  // force ninja Weapon
  cc_set_weapon(pCore, WEAPON_NINJA);

  pCore->m_Ninja.m_CurrentMoveTime--;

  if (pCore->m_Ninja.m_CurrentMoveTime == 0) {
    // reset velocity
    pCore->m_Vel =
        vfmul(pCore->m_Ninja.m_ActivationDir, pCore->m_Ninja.m_OldVelAmount);
  }

  if (pCore->m_Ninja.m_CurrentMoveTime > 0) {
    // Set velocity
    pCore->m_Vel = vfmul(pCore->m_Ninja.m_ActivationDir, NINJA_VELOCITY);
    vec2 OldPos = pCore->m_Pos;
    vec2 GroundElasticity = vec2_init(pCore->m_Tuning.m_GroundElasticityX,
                                      pCore->m_Tuning.m_GroundElasticityY);

    move_box(pCore->m_pCollision, &pCore->m_Pos, &pCore->m_Vel,
             GroundElasticity, NULL);

    pCore->m_Vel = vec2_init(0, 0);

    // check if we Hit anything along the way
    {
      float Radius = PHYSICALSIZE * 2.0f;

      // check that we're not in solo part
      if (pCore->m_Solo)
        return;

      for (int i = 0; i < pCore->m_pWorld->m_NumCharacters; ++i) {
        if (vdistance(OldPos, pCore->m_Pos) < Radius + PHYSICALSIZE) {
          SCharacterCore *pChr = &pCore->m_pWorld->m_pCharacters[i];
          if (pChr == pCore)
            continue;

          if (pChr->m_Solo)
            return;

          // make sure we haven't Hit this object before
          bool AlreadyHit = false;
          for (int j = 0; j < pCore->m_NumObjectsHit; j++) {
            if (pCore->m_aHitObjects[j] == pChr->m_Id)
              AlreadyHit = true;
          }
          if (AlreadyHit)
            continue;

          // check so we are sufficiently close
          if (vdistance(pChr->m_Pos, pCore->m_Pos) > (PHYSICALSIZE * 2.0f))
            continue;

          // set his velocity to fast upward (for now)
          if (pCore->m_NumObjectsHit < 10)
            pCore->m_aHitObjects[pCore->m_NumObjectsHit++] = pChr->m_Id;

          cc_take_damage(pChr, vec2_init(0, -10.f));
        }
      }
    }

    return;
  }
}

void cc_handle_jetpack(SCharacterCore *pCore) {
  vec2 Direction = vnormalize(vec2_init(pCore->m_LatestInput.m_TargetX,
                                        pCore->m_LatestInput.m_TargetY));

  bool FullAuto = false;
  if (pCore->m_ActiveWeapon == WEAPON_GRENADE ||
      pCore->m_ActiveWeapon == WEAPON_SHOTGUN ||
      pCore->m_ActiveWeapon == WEAPON_LASER)
    FullAuto = true;
  if (pCore->m_Jetpack && pCore->m_ActiveWeapon == WEAPON_GUN)
    FullAuto = true;

  // check if we gonna fire
  bool WillFire = false;
  if (count_input(pCore->m_LatestPrevInput.m_Fire, pCore->m_LatestInput.m_Fire)
          .m_Presses)
    WillFire = true;

  if (FullAuto && (pCore->m_LatestInput.m_Fire & 1))
    WillFire = true;

  if (!WillFire)
    return;

  if (pCore->m_FreezeTime) {
    return;
  }

  if (pCore->m_ActiveWeapon ==
      WEAPON_GUN) // put this up as weapon != gun: return
    if (pCore->m_Jetpack) {
      float Strength = pCore->m_Tuning.m_JetpackStrength;
      cc_take_damage(pCore, vfmul(Direction, -(Strength / 100.f / 6.11f)));
    }
}

void cc_do_weapon_switch(SCharacterCore *pCore) {

  // make sure we can switch
  if (pCore->m_ReloadTimer != 0 || pCore->m_QueuedWeapon == -1 ||
      pCore->m_aWeaponGot[WEAPON_NINJA] ||
      !pCore->m_aWeaponGot[pCore->m_QueuedWeapon])
    return;

  // switch Weapon
  cc_set_weapon(pCore, pCore->m_QueuedWeapon);
}

void wc_insert_entity(SWorldCore *pWorld, SEntity *pEnt);
void wc_remove_entity(SWorldCore *pWorld, SEntity *pEnt);

void cc_fire_weapon(SCharacterCore *pCore) {
  if (pCore->m_NumInputs < 2)
    return;
  if (pCore->m_ReloadTimer)
    return;
  cc_do_weapon_switch(pCore);

  if (pCore->m_FreezeTime)
    return;

  vec2 MouseTarget =
      vec2_init(pCore->m_LatestInput.m_TargetX, pCore->m_LatestInput.m_TargetY);
  vec2 Direction = vnormalize(MouseTarget);

  bool FullAuto = false;
  if (pCore->m_ActiveWeapon == WEAPON_GRENADE ||
      pCore->m_ActiveWeapon == WEAPON_SHOTGUN ||
      pCore->m_ActiveWeapon == WEAPON_LASER)
    FullAuto = true;
  if (pCore->m_Jetpack && pCore->m_ActiveWeapon == WEAPON_GUN)
    FullAuto = true;
  if (pCore->m_FrozenLastTick)
    FullAuto = true;

  // don't fire hammer when player is deep and sv_deepfly is disabled
  if (!pCore->m_pWorld->m_pConfig->m_SvDeepfly &&
      pCore->m_ActiveWeapon == WEAPON_HAMMER && pCore->m_DeepFrozen)
    return;

  // check if we gonna fire
  bool WillFire = false;
  if (count_input(pCore->m_LatestPrevInput.m_Fire, pCore->m_LatestInput.m_Fire)
          .m_Presses)
    WillFire = true;

  if (FullAuto && (pCore->m_LatestInput.m_Fire & 1))
    WillFire = true;

  if (!WillFire)
    return;

  vec2 ProjStartPos =
      vvadd(pCore->m_Pos, vfmul(Direction, PHYSICALSIZE * 0.75f));

  switch (pCore->m_ActiveWeapon) {
  case WEAPON_HAMMER: {
    // reset objects Hit
    pCore->m_NumObjectsHit = 0;

    if (pCore->m_HammerHitDisabled)
      break;
    if (pCore->m_Solo)
      break;

    int Hits = 0;
    for (int i = 0; i < pCore->m_pWorld->m_NumCharacters; ++i) {
      if (vdistance(pCore->m_pWorld->m_pCharacters[i].m_Pos, pCore->m_Pos) <
          (PHYSICALSIZE * 0.5f) + PHYSICALSIZE) {
        SCharacterCore *pTarget = &pCore->m_pWorld->m_pCharacters[i];

        if (pTarget == pCore || pTarget->m_Solo)
          continue;

        // set his velocity to fast upward (for now)

        vec2 Dir;
        if (vlength(vvsub(pTarget->m_Pos, pCore->m_Pos)) > 0.0f)
          Dir = vnormalize(vvsub(pTarget->m_Pos, pCore->m_Pos));
        else
          Dir = vec2_init(0.f, -1.f);

        float Strength = pCore->m_Tuning.m_HammerStrength;

        vec2 Temp =
            vvadd(pTarget->m_Vel,
                  vfmul(vnormalize(vvadd(Dir, vec2_init(0.f, -1.1f))), 10.0f));
        Temp =
            vvsub(clamp_vel(pTarget->m_MoveRestrictions, Temp), pTarget->m_Vel);

        vec2 Force = vfmul(vvadd(vec2_init(0.f, -1.0f), Temp), Strength);

        cc_take_damage(pTarget, Force);
        cc_unfreeze(pTarget);

        Hits++;
      }
    }

    // if we Hit anything, we have to wait for the reload
    if (Hits) {
      float FireDelay = pCore->m_Tuning.m_HammerHitFireDelay;
      pCore->m_ReloadTimer = FireDelay * SERVER_TICK_SPEED / 1000;
    }
  } break;

  case WEAPON_GUN: {
    if (!pCore->m_Jetpack) {
      // TODO: idk about this xd. bullets are useless in ddrace
      // int Lifetime =
      //     (int)(SERVER_TICK_SPEED *
      //     pCore->m_Tuning.m_GunLifetime);
      // new CProjectile(GameWorld(),
      //                 WEAPON_GUN,   // Type
      //                 GetCid(),     // Owner
      //                 ProjStartPos, // Pos
      //                 Direction,    // Dir
      //                 Lifetime,     // Span
      //                 false,        // Freeze
      //                 false,        // Explosive
      //                 0,            // Force
      //                 -1            // SoundImpact
      // );
    }
  } break;

  case WEAPON_SHOTGUN: {
    // float LaserReach = pCore->m_Tuning.m_LaserReach;
    // new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCid(),
    //            WEAPON_SHOTGUN);
    break;
  }

  case WEAPON_GRENADE: {
    int Lifetime = (int)(SERVER_TICK_SPEED * pCore->m_Tuning.m_GrenadeLifetime);
    SProjectile *pNewProj = malloc(sizeof(SProjectile));
    prj_init(pNewProj, pCore->m_pWorld, WEAPON_GRENADE, pCore->m_Id,
             ProjStartPos, Direction, Lifetime, false, true, MouseTarget, 0, 0);
    wc_insert_entity(pCore->m_pWorld, (SEntity *)pNewProj);
  } break;

  case WEAPON_LASER: {
    // float LaserReach = pCore->m_Tuning.m_LaserReach;
    // TODO:
    // new CLaser(GameWorld(), m_Pos, Direction, LaserReach, GetCid(),
    //            WEAPON_LASER);
  } break;

  case WEAPON_NINJA: {
    // reset Hit objects
    pCore->m_NumObjectsHit = 0;

    pCore->m_Ninja.m_ActivationDir = Direction;
    pCore->m_Ninja.m_CurrentMoveTime =
        (NINJA_MOVETIME * SERVER_TICK_SPEED) / 1000;
    pCore->m_Ninja.m_OldVelAmount = vlength(pCore->m_Vel);
  } break;
  }

  // reloadtimer can be changed earlier by hammer so check again
  if (!pCore->m_ReloadTimer) {
    pCore->m_ReloadTimer =
        (*(&pCore->m_Tuning.m_HammerFireDelay + pCore->m_ActiveWeapon) *
         SERVER_TICK_SPEED) /
        1000;
  }
}

void cc_handle_weapons(SCharacterCore *pCore) {
  if (pCore->m_ActiveWeapon == WEAPON_NINJA)
    cc_handle_ninja(pCore);
  cc_handle_jetpack(pCore);
  if (pCore->m_ReloadTimer) {
    --pCore->m_ReloadTimer;
    return;
  }
  cc_fire_weapon(pCore);
}

void cc_tick(SCharacterCore *pCore) {
  if (pCore->m_pWorld->m_NoWeakHookAndBounce) {
    cc_tick_deferred(pCore);
  } else {
    cc_pre_tick(pCore);
  }

  // handle Weapons
  cc_handle_weapons(pCore);

  cc_ddrace_postcore_tick(pCore);

  pCore->m_PrevPos = pCore->m_Pos;
}

void cc_handle_weapon_switch(SCharacterCore *pCore) {
  if (pCore->m_NumInputs < 2)
    return;
  int WantedWeapon = pCore->m_ActiveWeapon;
  if (pCore->m_QueuedWeapon != -1)
    WantedWeapon = pCore->m_QueuedWeapon;

  // Direct Weapon selection
  if (pCore->m_LatestInput.m_WantedWeapon)
    WantedWeapon = pCore->m_Input.m_WantedWeapon;

  // check for insane values
  if (WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS &&
      WantedWeapon != pCore->m_ActiveWeapon &&
      pCore->m_aWeaponGot[WantedWeapon])
    pCore->m_QueuedWeapon = WantedWeapon;

  cc_do_weapon_switch(pCore);
}

void cc_on_input(SCharacterCore *pCore, const SPlayerInput *pNewInput) {
  pCore->m_NumInputs++;
  pCore->m_LatestPrevInput = pCore->m_LatestInput;
  pCore->m_LatestInput = *pNewInput;

  if (pCore->m_LatestInput.m_TargetX == 0 &&
      pCore->m_LatestInput.m_TargetY == 0)
    pCore->m_LatestInput.m_TargetY = -1;

  if (pCore->m_NumInputs > 1) {
    cc_handle_weapon_switch(pCore);
    cc_fire_weapon(pCore);
  }

  pCore->m_LatestPrevInput = pCore->m_LatestInput;
}

void cc_on_predicted_input(SCharacterCore *pCore, SPlayerInput *pNewInput) {
  pCore->m_Input = *pNewInput;
  // it is not allowed to aim in the center
  if (pCore->m_Input.m_TargetX == 0 && pCore->m_Input.m_TargetY == 0)
    pCore->m_Input.m_TargetY = -1;
  pCore->m_SavedInput = pCore->m_Input;
}

// }}}

// WorldCore functions {{{

void init_switchers(SWorldCore *pCore, int HighestSwitchNumber) {
  if (HighestSwitchNumber > 0) {
    free(pCore->m_vSwitches);
    pCore->m_NumSwitches = HighestSwitchNumber + 1;
    pCore->m_vSwitches = malloc(pCore->m_NumSwitches * sizeof(SSwitch));
  } else {
    free(pCore->m_vSwitches);
    pCore->m_vSwitches = NULL;
    return;
  }

  for (int i = 0; i < pCore->m_NumSwitches; ++i) {
    pCore->m_vSwitches[i] = (SSwitch){.m_Initial = true,
                                      .m_Status = true,
                                      .m_EndTick = 0,
                                      .m_Type = 0,
                                      .m_LastUpdateTick = 0};
  }
}

// NOTE: spawn points are not the same as in ddnet. other players will not be
// respected
bool wc_next_spawn(SWorldCore *pCore, vec2 *pOutPos) {
  int Num;
  const vec2 *pSpawnPoints = spawn_points(pCore->m_pCollision, &Num);
  if (!pSpawnPoints)
    return false;
  *pOutPos = vfadd(vfmul(pSpawnPoints[pCore->m_GameTick % Num], 32), 16);
  return true;
}

void wc_release_hooked(SWorldCore *pCore, int Id) {
  for (int i = 0; i < pCore->m_NumCharacters; ++i)
    if (pCore->m_pCharacters[i].m_HookedPlayer == Id)
      cc_release_hook(
          &pCore->m_pCharacters[pCore->m_pCharacters[i].m_HookedPlayer]);
}

bool wc_on_entity(SWorldCore *pCore, int Index, int x, int y, int Layer,
                  int Flags, int Number) {
  const vec2 Pos = vec2_init(x * 32.0f + 16.0f, y * 32.0f + 16.0f);

  int aSides[8];
  aSides[0] = entity(pCore->m_pCollision, x, y + 1, Layer);
  aSides[1] = entity(pCore->m_pCollision, x + 1, y + 1, Layer);
  aSides[2] = entity(pCore->m_pCollision, x + 1, y, Layer);
  aSides[3] = entity(pCore->m_pCollision, x + 1, y - 1, Layer);
  aSides[4] = entity(pCore->m_pCollision, x, y - 1, Layer);
  aSides[5] = entity(pCore->m_pCollision, x - 1, y - 1, Layer);
  aSides[6] = entity(pCore->m_pCollision, x - 1, y, Layer);
  aSides[7] = entity(pCore->m_pCollision, x - 1, y + 1, Layer);

  if (Index == ENTITY_DOOR) {
    for (int i = 0; i < 8; i++) {
      if (aSides[i] >= ENTITY_LASER_SHORT && aSides[i] <= ENTITY_LASER_LONG) {
        // TODO: DOORS
        // new CDoor(&GameServer()->m_World, // GameWorld
        //           Pos,                    // Pos
        //           pi / 4 * i,             // Rotation
        //           32 * 3 + 32 * (aSides[i] - ENTITY_LASER_SHORT) * 3, //
        //           Length Number // Number
        // );
      }
    }
  } else if (Index == ENTITY_CRAZY_SHOTGUN_EX) {
    int Dir;
    if (!Flags)
      Dir = 0;
    else if (Flags == ROTATION_90)
      Dir = 1;
    else if (Flags == ROTATION_180)
      Dir = 2;
    else
      Dir = 3;
    float Deg = Dir * (PI / 2);
    SProjectile *pBullet = malloc(sizeof(SProjectile));
    prj_init(pBullet, pCore,
             WEAPON_SHOTGUN,                // Type
             -1,                            // Owner
             Pos,                           // Pos
             vec2_init(sin(Deg), cos(Deg)), // Dir
             -2,                            // Span
             true,                          // Freeze
             true,                          // Explosive
             vec2_init(sin(Deg), cos(Deg)), // InitDir
             Layer, Number);
    pBullet->m_Bouncing = 2 - (Dir % 2);
    wc_insert_entity(pCore, (SEntity *)pBullet);
  } else if (Index == ENTITY_CRAZY_SHOTGUN) {
    int Dir;
    if (!Flags)
      Dir = 0;
    else if (Flags == (TILEFLAG_ROTATE))
      Dir = 1;
    else if (Flags == (TILEFLAG_XFLIP | TILEFLAG_YFLIP))
      Dir = 2;
    else
      Dir = 3;
    float Deg = Dir * (PI / 2);
    SProjectile *pBullet = malloc(sizeof(SProjectile));
    prj_init(pBullet, pCore,
             WEAPON_SHOTGUN,                // Type
             -1,                            // Owner
             Pos,                           // Pos
             vec2_init(sin(Deg), cos(Deg)), // Dir
             -2,                            // Span
             true,                          // Freeze
             false,                         // Explosive
             vec2_init(sin(Deg), cos(Deg)), // InitDir
             Layer, Number);
    pBullet->m_Bouncing = 2 - (Dir % 2);
    wc_insert_entity(pCore, (SEntity *)pBullet);
  }

  int Type = -1;
  int SubType = 0;

  if (Index == ENTITY_ARMOR_1)
    Type = POWERUP_ARMOR;
  else if (Index == ENTITY_ARMOR_SHOTGUN)
    Type = POWERUP_ARMOR_SHOTGUN;
  else if (Index == ENTITY_ARMOR_GRENADE)
    Type = POWERUP_ARMOR_GRENADE;
  else if (Index == ENTITY_ARMOR_NINJA)
    Type = POWERUP_ARMOR_NINJA;
  else if (Index == ENTITY_ARMOR_LASER)
    Type = POWERUP_ARMOR_LASER;
  else if (Index == ENTITY_HEALTH_1)
    Type = POWERUP_HEALTH;
  else if (Index == ENTITY_WEAPON_SHOTGUN) {
    Type = POWERUP_WEAPON;
    SubType = WEAPON_SHOTGUN;
  } else if (Index == ENTITY_WEAPON_GRENADE) {
    Type = POWERUP_WEAPON;
    SubType = WEAPON_GRENADE;
  } else if (Index == ENTITY_WEAPON_LASER) {
    Type = POWERUP_WEAPON;
    SubType = WEAPON_LASER;
  } else if (Index == ENTITY_POWERUP_NINJA) {
    Type = POWERUP_NINJA;
    SubType = WEAPON_NINJA;
  } else if (Index >= ENTITY_LASER_FAST_CCW && Index <= ENTITY_LASER_FAST_CW) {
    // TODO: IMPLEMENT LIGHTS
    // int aSides2[8];
    // aSides2[0] = entity(pCore->m_pCollision, x, y + 2, Layer);
    // aSides2[1] = entity(pCore->m_pCollision, x + 2, y + 2, Layer);
    // aSides2[2] = entity(pCore->m_pCollision, x + 2, y, Layer);
    // aSides2[3] = entity(pCore->m_pCollision, x + 2, y - 2, Layer);
    // aSides2[4] = entity(pCore->m_pCollision, x, y - 2, Layer);
    // aSides2[5] = entity(pCore->m_pCollision, x - 2, y - 2, Layer);
    // aSides2[6] = entity(pCore->m_pCollision, x - 2, y, Layer);
    // aSides2[7] = entity(pCore->m_pCollision, x - 2, y + 2, Layer);
    // int Ind = Index - ENTITY_LASER_STOP;
    // int M;
    // if (Ind < 0) {
    //   Ind = -Ind;
    //   M = 1;
    // } else if (Ind == 0)
    //   M = 0;
    // else
    //   M = -1;
    // float AngularSpeed = 0.0f;
    // if (Ind == 0)
    //   AngularSpeed = 0.0f;
    // else if (Ind == 1)
    //   AngularSpeed = PI / 360;
    // else if (Ind == 2)
    //   AngularSpeed = PI / 180;
    // else if (Ind == 3)
    //   AngularSpeed = PI / 90;
    // AngularSpeed *= M;
    // for (int i = 0; i < 8; i++) {
    //   if (aSides[i] >= ENTITY_LASER_SHORT && aSides[i] <=
    //   ENTITY_LASER_LONG)
    //   {
    //     CLight *pLight = new CLight(
    //         &GameServer()->m_World, Pos, pi / 4 * i,
    //         32 * 3 + 32 * (aSides[i] - ENTITY_LASER_SHORT) * 3, Layer,
    //         Number);
    //     pLight->m_AngularSpeed = AngularSpeed;
    //     if (aSides2[i] >= ENTITY_LASER_C_SLOW &&
    //         aSides2[i] <= ENTITY_LASER_C_FAST) {
    //       pLight->m_Speed = 1 + (aSides2[i] - ENTITY_LASER_C_SLOW) * 2;
    //       pLight->m_CurveLength = pLight->m_Length;
    //     } else if (aSides2[i] >= ENTITY_LASER_O_SLOW &&
    //                aSides2[i] <= ENTITY_LASER_O_FAST) {
    //       pLight->m_Speed = 1 + (aSides2[i] - ENTITY_LASER_O_SLOW) * 2;
    //       pLight->m_CurveLength = 0;
    //     } else
    //       pLight->m_CurveLength = pLight->m_Length;
    //   }
    // }
    // TODO: implement draggers + plasma
  } else if (Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG) {
    // new CDragger(&GameServer()->m_World, Pos, Index - ENTITY_DRAGGER_WEAK +
    // 1,
    //              false, Layer, Number);
  } else if (Index >= ENTITY_DRAGGER_WEAK_NW &&
             Index <= ENTITY_DRAGGER_STRONG_NW) {
    // new CDragger(&GameServer()->m_World, Pos,
    //              Index - ENTITY_DRAGGER_WEAK_NW + 1, true, Layer, Number);
  } else if (Index == ENTITY_PLASMAE) {
    // new CGun(&GameServer()->m_World, Pos, false, true, Layer, Number);
  } else if (Index == ENTITY_PLASMAF) {
    // new CGun(&GameServer()->m_World, Pos, true, false, Layer, Number);
  } else if (Index == ENTITY_PLASMA) {
    // new CGun(&GameServer()->m_World, Pos, true, true, Layer, Number);
  } else if (Index == ENTITY_PLASMAU) {
    // new CGun(&GameServer()->m_World, Pos, false, false, Layer, Number);
  }

  if (Type != -1) {
    SPickup *pPickup = &pCore->m_pPickups[pCore->m_NumPickups++];
    pick_init(pPickup, pCore, Type, SubType, Layer, Number);
    pPickup->m_Pos = Pos;
    return true;
  }

  return false;
}

void wc_create_all_entities(SWorldCore *pCore) {
  for (int y = 0; y < pCore->m_pCollision->m_MapData.m_Height; y++) {
    for (int x = 0; x < pCore->m_pCollision->m_MapData.m_Width; x++) {
      const int Index = y * pCore->m_pCollision->m_MapData.m_Width + x;

      // Game layer
      {
        const int GameIndex =
            pCore->m_pCollision->m_MapData.m_GameLayer.m_pData[Index];
        if (GameIndex == TILE_OLDLASER) {
          pCore->m_pConfig->m_SvOldLaser = 1;
        } else if (GameIndex == TILE_NPC) {
          pCore->m_pTuningList[0].m_PlayerCollision = 0;
        } else if (GameIndex == TILE_EHOOK) {
          pCore->m_pConfig->m_SvEndlessDrag = 1;
        } else if (GameIndex == TILE_NOHIT) {
          pCore->m_pConfig->m_SvHit = 0;
        } else if (GameIndex == TILE_NPH) {
          pCore->m_pTuningList[0].m_PlayerHooking = 0;
        } else if (GameIndex >= ENTITY_OFFSET) {
          wc_on_entity(
              pCore, GameIndex - ENTITY_OFFSET, x, y, LAYER_GAME,
              pCore->m_pCollision->m_MapData.m_GameLayer.m_pFlags[Index], 0);
        }
      }

      if (pCore->m_pCollision->m_MapData.m_FrontLayer.m_pData) {
        const int FrontIndex =
            pCore->m_pCollision->m_MapData.m_FrontLayer.m_pData[Index];
        if (FrontIndex == TILE_OLDLASER) {
          pCore->m_pConfig->m_SvOldLaser = 1;
        } else if (FrontIndex == TILE_NPC) {
          pCore->m_pTuningList[0].m_PlayerCollision = 0;
        } else if (FrontIndex == TILE_EHOOK) {
          pCore->m_pConfig->m_SvEndlessDrag = 1;
        } else if (FrontIndex == TILE_NOHIT) {
          pCore->m_pConfig->m_SvHit = 0;
        } else if (FrontIndex == TILE_NPH) {
          pCore->m_pTuningList[0].m_PlayerHooking = 0;
        } else if (FrontIndex >= ENTITY_OFFSET) {
          wc_on_entity(
              pCore, FrontIndex - ENTITY_OFFSET, x, y, LAYER_FRONT,
              pCore->m_pCollision->m_MapData.m_FrontLayer.m_pFlags[Index], 0);
        }
      }

      if (pCore->m_pCollision->m_MapData.m_SwitchLayer.m_pType) {
        const int SwitchType =
            pCore->m_pCollision->m_MapData.m_SwitchLayer.m_pType[Index];
        if (SwitchType >= ENTITY_OFFSET) {
          wc_on_entity(
              pCore, SwitchType - ENTITY_OFFSET, x, y, LAYER_SWITCH,
              pCore->m_pCollision->m_MapData.m_SwitchLayer.m_pFlags[Index],
              pCore->m_pCollision->m_MapData.m_SwitchLayer.m_pNumber[Index]);
        }
      }
    }
  }
}

void wc_init(SWorldCore *pCore, SCollision *pCollision, SConfig *pConfig) {
  memset(pCore, 0, sizeof(SWorldCore));
  pCore->m_pCollision = pCollision;
  pCore->m_pConfig = pConfig;

  // TODO: figure out highest switch number in collision
  init_switchers(pCore, 0);

  // TODO: figure out the amount of tune zones in collision
  pCore->m_NumTuneZones = 1;
  pCore->m_pTuningList = malloc(pCore->m_NumTuneZones * sizeof(STuningParams));
  for (int i = 0; i < pCore->m_NumTuneZones; ++i)
    init_tuning_params(&pCore->m_pTuningList[i]);

  // configs
  pCore->m_NoWeakHook = false;
  pCore->m_NoWeakHookAndBounce = false;

  pCore->m_pPickups = malloc(pCollision->m_NumPickupsTotal * sizeof(SPickup));

  wc_create_all_entities(pCore);
}

void wc_free(SWorldCore *pCore) {
  for (int i = 0; i < NUM_ENTTYPES; ++i) {
    SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
    while (pEntity) {
      SEntity *pFree = pEntity;
      pEntity = pEntity->m_pNextTypeEntity;
      free(pFree);
    }
  }
  free(pCore->m_pTuningList);
  free(pCore->m_pCharacters);
}

void wc_tick(SWorldCore *pCore) {
  ++pCore->m_GameTick;
  for (int i = 0; i < pCore->m_NumCharacters; ++i)
    cc_on_predicted_input(&pCore->m_pCharacters[i],
                          &pCore->m_pCharacters[i].m_LatestInput);

  // Do Tick
  {
    // Tick projectiles
    SEntity *pEntity = pCore->m_apFirstEntityTypes[ENTTYPE_PROJECTILE];
    while (pEntity) {
      prj_tick((SProjectile *)pEntity);
      pEntity = pEntity->m_pNextTypeEntity;
    }

    // TODO: do lasers!!! aka. like 10 different entities that all identify as
    // lasers
    // Tick lasers
    // pEntity = pCore->m_apFirstEntityTypes[ENTTYPE_LASER];
    // while (pEntity) {
    //   laser_tick((SLaser *)pEntity);
    //   pEntity = pEntity->m_pNextTypeEntity;
    // }

    // Tick pickups
    for (int i = 0; i < pCore->m_NumPickups; ++i)
      pick_tick(&pCore->m_pPickups[i]);

    // Tick characters
    if (pCore->m_NoWeakHook) {
      for (int i = 0; i < pCore->m_NumCharacters; ++i)
        cc_pre_tick((SCharacterCore *)&pCore->m_pCharacters[i]);
    }
    for (int i = 0; i < pCore->m_NumCharacters; ++i)
      cc_tick((SCharacterCore *)&pCore->m_pCharacters[i]);
  }

  // Do tick deferred
  // funny thing no other entities than the character actually have a deferred
  // tick function lol
  for (int i = 0; i < pCore->m_NumCharacters; ++i)
    cc_world_tick_deferred(&pCore->m_pCharacters[i]);

  // Remove all entities that are marked for destroy
  for (int i = 0; i < NUM_ENTTYPES; ++i) {
    SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
    while (pEntity) {
      SEntity *pFree = pEntity;
      pEntity = pEntity->m_pNextTypeEntity;
      if (pFree->m_MarkedForDestroy) {
        wc_remove_entity(pCore, pFree);
        free(pFree);
      }
    }
  }
}

SCharacterCore *wc_add_character(SWorldCore *pWorld) {
  const int NewSize = pWorld->m_NumCharacters + 1;
  SCharacterCore *pNewArray =
      realloc(pWorld->m_pCharacters, NewSize * sizeof(SCharacterCore));
  if (!pNewArray)
    return NULL;

  pWorld->m_pCharacters = pNewArray;
  SCharacterCore *pChar = &pWorld->m_pCharacters[pWorld->m_NumCharacters];
  pWorld->m_NumCharacters = NewSize;

  cc_init(pChar, pWorld);
  pChar->m_Id = pWorld->m_NumCharacters - 1;

  vec2 SpawnPos;
  if (wc_next_spawn(pWorld, &SpawnPos)) {
    pChar->m_Pos = SpawnPos;
    pChar->m_PrevPos = SpawnPos;
  }

  return pChar;
}

void wc_create_explosion(SWorldCore *pWorld, vec2 Pos, int Owner) {
#define EXPLOSION_RADIUS 135.0f
#define EXPLOSION_INNER_RADIUS 48.0f
  for (int i = 0; i < pWorld->m_NumCharacters; i++) {
    SCharacterCore *pChr = &pWorld->m_pCharacters[i];
    vec2 Diff = vvsub(pChr->m_Pos, Pos);
    float l = vlength(Diff);
    if (l >= EXPLOSION_RADIUS + PHYSICALSIZE)
      continue;
    vec2 ForceDir = vec2_init(0, 1);
    if (l)
      ForceDir = vnormalize(Diff);
    l = 1 - fclamp((l - EXPLOSION_INNER_RADIUS) /
                       (EXPLOSION_RADIUS - EXPLOSION_INNER_RADIUS),
                   0.0f, 1.0f);
    float Strength;
    if (Owner != -1)
      Strength = pWorld->m_pCharacters[Owner].m_Tuning.m_ExplosionStrength;
    else
      Strength = pWorld->m_pTuningList[0].m_ExplosionStrength;

    float Dmg = Strength * l;
    if (!(int)Dmg)
      continue;

    if (!pWorld->m_pCharacters[Owner].m_GrenadeHitDisabled ||
        Owner == pChr->m_Id) {
      if (pChr->m_Solo && Owner != pChr->m_Id)
        continue;
      cc_take_damage(pChr, vfmul(ForceDir, Dmg * 2));
    }
  }
}

SCharacterCore *wc_intersect_character(SWorldCore *pWorld, vec2 Pos0, vec2 Pos1,
                                       float Radius, vec2 *pNewPos,
                                       const SCharacterCore *pNotThis,
                                       const SCharacterCore *pThisOnly) {
  float ClosestLen = vdistance(Pos0, Pos1) * 100.0f;
  SCharacterCore *pClosest = NULL;

  for (int i = 0; i < pWorld->m_NumCharacters; ++i) {
    SCharacterCore *pEntity = &pWorld->m_pCharacters[i];
    if (pEntity == pNotThis)
      continue;

    if (pThisOnly && pEntity != pThisOnly)
      continue;

    vec2 IntersectPos;
    if (closest_point_on_line(Pos0, Pos1, pEntity->m_Pos, &IntersectPos)) {
      float Len = vdistance(pEntity->m_Pos, IntersectPos);
      if (Len < PHYSICALSIZE + Radius) {
        Len = vdistance(Pos0, IntersectPos);
        if (Len < ClosestLen) {
          *pNewPos = IntersectPos;
          ClosestLen = Len;
          pClosest = pEntity;
        }
      }
    }
  }

  return pClosest;
}

void wc_insert_entity(SWorldCore *pWorld, SEntity *pEnt) {
  if (pWorld->m_apFirstEntityTypes[pEnt->m_ObjType])
    pWorld->m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
  pEnt->m_pNextTypeEntity = pWorld->m_apFirstEntityTypes[pEnt->m_ObjType];
  pEnt->m_pPrevTypeEntity = NULL;
  pWorld->m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void wc_remove_entity(SWorldCore *pWorld, SEntity *pEnt) {
  if (!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity &&
      pWorld->m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
    return;

  if (pEnt->m_pPrevTypeEntity)
    pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
  else
    pWorld->m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
  if (pEnt->m_pNextTypeEntity)
    pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

  if (pWorld->m_pNextTraverseEntity == pEnt)
    pWorld->m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

  pEnt->m_pNextTypeEntity = NULL;
  pEnt->m_pPrevTypeEntity = NULL;
}

// }}}
