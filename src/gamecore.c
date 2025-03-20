#include "gamecore.h"
#include "collision.h"
#include "vmath.h"
#include <stdlib.h>
#include <string.h>

static void init_tuning_params(STuningParams *pTunings) {
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description)               \
  pTunings->m_##Name.m_Value = Value * 100.f;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
}
inline static float tune_get(STuneParam Tune) { return Tune.m_Value / 100.f; }

// Physics helper functions {{{

enum {
  CANTMOVE_LEFT = 1 << 0,
  CANTMOVE_RIGHT = 1 << 1,
  CANTMOVE_UP = 1 << 2,
  CANTMOVE_DOWN = 1 << 3,
};

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
struct InputCount {
  int m_Presses;
  int m_Releases;
} typedef SInputCount;

static inline SInputCount CountInput(int Prev, int Cur) {
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
  if (!pCore->m_aWeapons[pCore->m_ActiveWeapon].m_Got)
    pCore->m_ActiveWeapon = WEAPON_GUN;
  pCore->m_FreezeTime = 0;
  pCore->m_FrozenLastTick = true;
}
bool is_switch_active_cb(int Number, void *pUser) {
  SCharacterCore *pThis = (SCharacterCore *)pUser;
  return pThis->m_pWorld->m_vSwitches &&
         pThis->m_pWorld->m_vSwitches[Number].m_Status;
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
             tune_get(pCore->m_Tuning.m_PlayerCollision));

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
            pCore->m_Tuning.m_PlayerHooking.m_Value) {
          if (Distance > PHYSICALSIZE * 1.50f) {
            float HookAccel =
                tune_get(pCore->m_Tuning.m_HookDragAccel) *
                (Distance / tune_get(pCore->m_Tuning.m_HookLength));
            float DragSpeed = tune_get(pCore->m_Tuning.m_HookDragSpeed);

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
       get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                              pCore->m_Pos.y - DEATH) == TILE_DEATH ||
       get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x + DEATH,
                              pCore->m_Pos.y + DEATH) == TILE_DEATH ||
       get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                              pCore->m_Pos.y - DEATH) == TILE_DEATH ||
       get_front_collision_at(pCore->m_pCollision, pCore->m_Pos.x - DEATH,
                              pCore->m_Pos.y + DEATH) == TILE_DEATH)) {
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
        pCore->m_Vel = vfmul(Direction, (MaxSpeed / 5));
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
            tune_get(pCore->m_Tuning.m_VelrampRange) /
            (50 * log(fmax((float)tune_get(pCore->m_Tuning.m_VelrampCurvature),
                           1.01f)));
        MaxSpeed =
            fmax(MaxRampSpeed, tune_get(pCore->m_Tuning.m_VelrampStart) / 50) *
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

void cc_handle_tiles(SCharacterCore *pCore, int Index) {
  int MapIndex = Index;
  int TileIndex = get_tile_index(pCore->m_pCollision, MapIndex);
  int TileFIndex = get_front_tile_index(pCore->m_pCollision, MapIndex);
  pCore->m_MoveRestrictions = get_move_restrictions(
      is_switch_active_cb, pCore, pCore->m_Pos, 18.0f, MapIndex);
  if (Index < 0) {
    pCore->m_LastRefillJumps = false;
    pCore->m_LastPenalty = false;
    pCore->m_LastBonus = false;
    return;
  }
  int TeleCheckpoint = is_tele_checkpoint(pCore->m_pCollision, MapIndex);
  if (TeleCheckpoint)
    pCore->m_TeleCheckpoint = TeleCheckpoint;

  // freeze
  if ((TileIndex == TILE_FREEZE || TileFIndex == TILE_FREEZE) &&
      !pCore->m_DeepFrozen) {
    cc_freeze(pCore, 3);
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
      !m_LastRefillJumps) {
    pCore->m_JumpedTotal = 0;
    pCore->m_Jumped = 0;
    m_LastRefillJumps = true;
  }
  if (TileIndex != TILE_REFILL_JUMPS && TileFIndex != TILE_REFILL_JUMPS) {
    m_LastRefillJumps = false;
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
  if (pCore->m_Vel.y > 0 && (m_MoveRestrictions & CANTMOVE_DOWN)) {
    pCore->m_Jumped = 0;
    pCore->m_JumpedTotal = 0;
  }
  // apply move restrictions
  pCore->m_Vel = clamp_vel(pCore->m_MoveRestrictions, pCore->m_Vel);

  SSwitch *pSwitches = pCore->m_pWorld->m_vSwitches;
  if (pSwitches) {
    int Number = get_switch_number(pCore->m_pCollision, MapIndex);
    int Type = get_switch_type(pCore->m_pCollision, MapIndex);
    int Delay = get_switch_delay(pCore->m_pCollision, MapIndex);
    int Tick = pCore->m_pWorld->m_GameTick;

    // handle switch tiles
    if (Type == TILE_SWITCHOPEN && Number > 0) {
      pSwitches[Number].m_Status = true;
      pSwitches[Number].m_EndTick = 0;
      pSwitches[Number].m_Type = TILE_SWITCHOPEN;
      pSwitches[Number].m_aLastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHTIMEDOPEN && Number > 0) {
      pSwitches[Number].m_Status = true;
      pSwitches[Number].m_EndTick = Tick + 1 + Delay * SERVER_TICK_SPEED;
      pSwitches[Number].m_Type = TILE_SWITCHTIMEDOPEN;
      pSwitches[Number].m_aLastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHTIMEDCLOSE && Number > 0) {
      pSwitches[Number].m_Status = false;
      pSwitches[Number].m_EndTick = Tick + 1 + Delay * SERVER_TICK_SPEED;
      pSwitches[Number].m_Type = TILE_SWITCHTIMEDCLOSE;
      pSwitches[Number].m_aLastUpdateTick = Tick;
    } else if (Type == TILE_SWITCHCLOSE && Number > 0) {
      pSwitches[Number].m_Status = false;
      pSwitches[Number].m_EndTick = 0;
      pSwitches[Number].m_Type = TILE_SWITCHCLOSE;
      pSwitches[Number].m_aLastUpdateTick = Tick;
    } else if (Type == TILE_FREEZE) {
      if (Number == 0 || pSwitches[Number].m_Status) {
        Freeze(Delay);
      }
    } else if (Type == TILE_DFREEZE) {
      if (Number == 0 || pSwitches[Number].m_Status)
        pCore->m_DeepFrozen = true;
    } else if (Type == TILE_DUNFREEZE) {
      if (Number == 0 || pSwitches[Number].m_Status)
        pCore->m_DeepFrozen = false;
    } else if (Type == TILE_LFREEZE) {
      if (Number == 0 || pSwitches[Number].m_Status) {
        pCore->m_LiveFrozen = true;
      }
    } else if (Type == TILE_LUNFREEZE) {
      if (Number == 0 || pSwitches[Number].m_Status) {
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
    } else if (Type == TILE_ADD_TIME && !m_LastPenalty) {
      int min = Delay;
      int sec = Number;
      int Team = Teams()->pCore->Team(pCore->m_Id);
      pCore->m_StartTime -= (min * 60 + sec) * SERVER_TICK_SPEED;
      pCore->m_LastPenalty = true;
    } else if (Type == TILE_SUBTRACT_TIME && !m_LastBonus) {
      int min = Delay;
      int sec = Number;
      int Team = Teams()->pCore->Team(pCore->m_Id);
      pCore->m_StartTime += (min * 60 + sec) * SERVER_TICK_SPEED;
      if (m_StartTime > Tick)
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

  int z = is_teleport(pCore->m_pCollision, MapIndex);
  int Num;
  if (z && tele_outs(pCore->m_pCollision, z - 1, &Num) && Num > 0 &&
      !g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons) {

    // TODO: make this be controlled by player input later
    pCore->m_Pos = tele_outs(pCore->m_pCollision, z - 1,
                             &Num)[pCore->m_pWorld->m_GameTick % Num];
    if (!g_Config.m_SvTeleportHoldHook) {
      ResetHook();
    }
    if (g_Config.m_SvTeleportLoseWeapons)
      ResetPickups();
    return;
  }
  int evilz = is_evil_teleport(pCore->m_pCollision, MapIndex);
  if (evilz && tele_outs(pCore->m_pCollision, evilz, &Num) && Num > 0) {
    // TODO: make this be controlled by player input later
    pCore->m_Pos = tele_outs(pCore->m_pCollision,
                             evilz - 1)[pCore->m_pWorld->m_GameTick % Num];
    if (!g_Config.m_SvOldTeleportHook && !g_Config.m_SvOldTeleportWeapons) {
      pCore->m_Vel = vec2(0, 0);

      if (!g_Config.m_SvTeleportHoldHook) {
        ResetHook();
        GameWorld()->ReleaseHooked(GetPlayer()->GetCid());
      }
      if (g_Config.m_SvTeleportLoseWeapons) {
        ResetPickups();
      }
    }
    return;
  }
  if (Collision()->IsCheckEvilTeleport(MapIndex)) {
    // first check if there is a TeleCheckOut for the current recorded
    // checkpoint, if not check previous checkpoints
    for (int k = m_TeleCheckpoint - 1; k >= 0; k--) {
      if (!Collision()->TeleCheckOuts(k).empty()) {
        int TeleOut =
            GameWorld()->pCore->RandomOr0(Collision()->TeleCheckOuts(k).size());
        pCore->m_Pos = Collision()->TeleCheckOuts(k)[TeleOut];
        pCore->m_Vel = vec2(0, 0);

        if (!g_Config.m_SvTeleportHoldHook) {
          ResetHook();
          GameWorld()->ReleaseHooked(GetPlayer()->GetCid());
        }

        return;
      }
    }
    // if no checkpointout have been found (or if there no recorded checkpoint),
    // teleport to start
    vec2 SpawnPos;
    if (GameServer()->m_pController->CanSpawn(
            m_pPlayer->GetTeam(), &SpawnPos,
            GameServer()->GetDDRaceTeam(GetPlayer()->GetCid()))) {
      pCore->m_Pos = SpawnPos;
      pCore->m_Vel = vec2(0, 0);

      if (!g_Config.m_SvTeleportHoldHook) {
        ResetHook();
        GameWorld()->ReleaseHooked(GetPlayer()->GetCid());
      }
    }
    return;
  }
  if (Collision()->IsCheckTeleport(MapIndex)) {
    // first check if there is a TeleCheckOut for the current recorded
    // checkpoint, if not check previous checkpoints
    for (int k = m_TeleCheckpoint - 1; k >= 0; k--) {
      if (!Collision()->TeleCheckOuts(k).empty()) {
        int TeleOut =
            GameWorld()->pCore->RandomOr0(Collision()->TeleCheckOuts(k).size());
        pCore->m_Pos = Collision()->TeleCheckOuts(k)[TeleOut];

        if (!g_Config.m_SvTeleportHoldHook) {
          ResetHook();
        }

        return;
      }
    }
    // if no checkpointout have been found (or if there no recorded checkpoint),
    // teleport to start
    vec2 SpawnPos;
    if (GameServer()->m_pController->CanSpawn(
            m_pPlayer->GetTeam(), &SpawnPos,
            GameServer()->GetDDRaceTeam(GetPlayer()->GetCid()))) {
      pCore->m_Pos = SpawnPos;

      if (!g_Config.m_SvTeleportHoldHook) {
        ResetHook();
      }
    }
    return;
  }
}

void cc_ddrace_postcore_tick(SCharacterCore *pCore) {

  if (pCore->m_EndlessHook)
    pCore->m_HookTick = 0;

  pCore->m_FrozenLastTick = false;

  // hardcode 3s freeze for now
  if (pCore->m_DeepFrozen)
    cc_freeze(pCore, 3);

  // following jump rules can be overridden by tiles, like Refill Jumps, Stopper
  // and Wall Jump
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

  // handle Anti-Skip tiles
  vector<int> vIndices = Collision()->GetMapIndices(m_PrevPos, m_Pos);
  if (!vIndices.empty()) {
    for (int &Index : vIndices) {
      HandleTiles(Index);
      if (!m_Alive)
        return;
    }
  } else {
    HandleTiles(CurrentIndex);
    if (!m_Alive)
      return;
  }

  // teleport gun
  if (pCore->m_TeleGunTeleport) {
    pCore->m_Pos = m_TeleGunPos;
    if (!pCore->m_IsBlueTeleGunTeleport)
      pCore->m_Vel = vec2(0, 0);
    pCore->m_TeleGunTeleport = false;
    pCore->m_IsBlueTeleGunTeleport = false;
  }
}

void cc_pre_tick(SCharacterCore *pCore) {
  cc_ddracetick(pCore);

  pCore->m_MoveRestrictions = get_move_restrictions(
      pCore->m_pCollision, is_switch_active_cb, pCore, pCore->m_Pos);

  // get ground state
  const bool Grounded =
      check_point(pCore->m_pCollision, pCore->m_Pos.x + PHYSICALSIZE / 2,
                  pCore->m_Pos.y + PHYSICALSIZE / 2 + 5) ||
      check_point(pCore->m_pCollision, pCore->m_Pos.x - PHYSICALSIZE / 2,
                  pCore->m_Pos.y + PHYSICALSIZE / 2 + 5);

  vec2 TargetDirection =
      vnormalize((vec2){pCore->m_Input.m_TargetX, pCore->m_Input.m_TargetY});

  pCore->m_Vel.y += tune_get(pCore->m_Tuning.m_Gravity);

  float MaxSpeed = Grounded ? tune_get(pCore->m_Tuning.m_GroundControlSpeed)
                            : tune_get(pCore->m_Tuning.m_AirControlSpeed);
  float Accel = Grounded ? tune_get(pCore->m_Tuning.m_GroundControlAccel)
                         : tune_get(pCore->m_Tuning.m_AirControlAccel);
  float Friction = Grounded ? tune_get(pCore->m_Tuning.m_GroundFriction)
                            : tune_get(pCore->m_Tuning.m_AirFriction);

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
        pCore->m_Vel.y = -tune_get(pCore->m_Tuning.m_GroundJumpImpulse);
        if (pCore->m_Jumps > 1) {
          pCore->m_Jumped |= 1;
        } else {
          pCore->m_Jumped |= 3;
        }
        pCore->m_JumpedTotal = 0;
      } else if (!(pCore->m_Jumped & 2)) {
        pCore->m_Vel.y = -tune_get(pCore->m_Tuning.m_AirJumpImpulse);
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
      pCore->m_HookTick = (float)SERVER_TICK_SPEED *
                          (1.25f - tune_get(pCore->m_Tuning.m_HookDuration));
    }
  } else {
    pCore->m_HookedPlayer = -1;
    pCore->m_HookState = HOOK_IDLE;
    pCore->m_HookPos = pCore->m_Pos;
  }

  // handle jumping
  // 1 bit = to keep track if a jump has been made on this input (player is
  // holding space bar) 2 bit = to track if all air-jumps have been used up (tee
  // gets dark feet)
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
    vec2 NewPos = vvadd(
        pCore->m_HookPos,
        vfmul(pCore->m_HookDir, tune_get(pCore->m_Tuning.m_HookFireSpeed)));
    if (vdistance(HookBase, NewPos) > tune_get(pCore->m_Tuning.m_HookLength)) {
      pCore->m_HookState = HOOK_RETRACT_START;
      NewPos = vvadd(HookBase, vfmul(vnormalize(vvsub(NewPos, HookBase)),
                                     tune_get(pCore->m_Tuning.m_HookLength)));
      pCore->m_Reset = true;
    }

    // make sure that the hook doesn't go though the ground
    bool GoingToHitGround = false;
    bool GoingToRetract = false;
    bool GoingThroughTele = false;
    int teleNr = 0;
    int Hit = intersect_line_tele_hook(pCore->m_pCollision, pCore->m_HookPos,
                                       NewPos, &NewPos, NULL, &teleNr);

    if (Hit) {
      if (Hit == TILE_NOHOOK)
        GoingToRetract = true;
      else if (Hit == TILE_TELEINHOOK)
        GoingThroughTele = true;
      else
        GoingToHitGround = true;
      pCore->m_Reset = true;
    }

    // Check against other players first
    if (!pCore->m_HookHitDisabled && pCore->m_pWorld &&
        pCore->m_Tuning.m_PlayerHooking.m_Value &&
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
      vec2 *pTeleOuts = tele_outs(pCore->m_pCollision, teleNr - 1, &NumOuts);
      if (GoingThroughTele && NumOuts > 0) {
        pCore->m_HookedPlayer = -1;

        pCore->m_NewHook = true;
        // TODO: add a proper system for this.
        // i don't want to use random number obviously since this is for
        // simulation purposes so the player should be able to control this with
        // an input
        pCore->m_HookPos = vvadd(pTeleOuts[pCore->m_pWorld->m_GameTick],
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
                           tune_get(pCore->m_Tuning.m_HookDragAccel));
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
      if (NewVelLength < tune_get(pCore->m_Tuning.m_HookDragSpeed) ||
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

void cc_tick(SCharacterCore *pCore) {
  if (pCore->m_pWorld->m_NoWeakHook) {

    cc_tick_deferred(pCore);
  } else {
    cc_pre_tick(pCore);
  }

  // handle Weapons
  HandleWeapons();

  DDRacePostCoreTick();

  // Previnput
  pCore->m_PrevInput = pCore->m_Input;

  pCore->m_PrevPos = pCore->m_Pos;
}

void cc_move(SCharacterCore *pCore) {}

void cc_quantize(SCharacterCore *pCore) {
  pCore->m_Pos.x = round_to_int(pCore->m_Pos.x);
  pCore->m_Pos.y = round_to_int(pCore->m_Pos.y);
  pCore->m_Vel.x = round_to_int(pCore->m_Vel.x * 256.0f) / 256.0f;
  pCore->m_Vel.y = round_to_int(pCore->m_Vel.y * 256.0f) / 256.0f;
  pCore->m_HookPos.x = round_to_int(pCore->m_HookPos.x);
  pCore->m_HookPos.y = round_to_int(pCore->m_HookPos.y);
  pCore->m_HookDir.x = round_to_int(pCore->m_HookDir.x * 256.0f) / 256.f;
  pCore->m_HookDir.y = round_to_int(pCore->m_HookDir.y * 256.0f) / 256.f;
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

void wc_init(SWorldCore *pCore, SCollision *pCollision) {
  memset(pCore, 0, sizeof(SWorldCore));
  pCore->m_pCollision = pCollision;

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
  free(pCore->m_pCharacters);
}

void wc_tick(SWorldCore *pCore) {

  // Do Tick
  for (int i = 0; i < NUM_ENTTYPES; ++i) {
    // TODO: implement all other entities
    switch (i) {
    case ENTTYPE_PROJECTILE: {
    }
    case ENTTYPE_LASER: {
    }
    case ENTTYPE_PICKUP: {
    }
    }
    if (pCore->m_NoWeakHook) {
      for (int i = 0; i < pCore->m_NumCharacters; ++i)
        cc_pre_tick((SCharacterCore *)&pCore->m_pCharacters[i]);
    }
  }
  for (int i = 0; i < pCore->m_NumCharacters; ++i)
    cc_tick((SCharacterCore *)&pCore->m_pCharacters[i]);

  // Do tick deferred
  for (int i = 0; i < NUM_ENTTYPES; ++i) {
    switch (i) {
    case ENTTYPE_PROJECTILE: {
      // SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
      // while (pEntity) {
      //   pj_tick_deferred((SProjectile *)pEntity);
      //   pEntity = pEntity->m_pNextTypeEntity;
      // }
    }
    case ENTTYPE_LASER: {
    }
    case ENTTYPE_PICKUP: {
    }
    }
  }
  for (int i = 0; i < pCore->m_NumCharacters; ++i)
    cc_tick_deferred((SCharacterCore *)&pCore->m_pCharacters[i]);
}

// }}}
