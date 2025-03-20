#include "gamecore.h"
#include "collision.h"
#include "vmath.h"
#include <stdlib.h>
#include <string.h>

static void init_tunings(STuningParams *pTunings) {
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description)               \
  pTunings->m_##Name.m_Value = Value * 100.f;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
}
inline static float get_tune(STuneParam Tune) { return Tune.m_Value / 100.f; }

// Physics helper functions {{{

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
  pCore->m_Base.m_pWorld = pWorld;
  pCore->m_Base.m_pCollision = pWorld->m_pCollision;

  // The world assigns ids to the core
  pCore->m_Base.m_Id = -1;
  init_tunings(&pCore->m_Tuning);
}

void cc_set_worldcore(SCharacterCore *pCore, SWorldCore *pWorld,
                      SCollision *pCollision) {
  pCore->m_Base.m_pWorld = pWorld;
  pCore->m_Base.m_pCollision = pCollision;
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
  return pThis->m_Base.m_pWorld->m_vSwitches &&
         pThis->m_Base.m_pWorld->m_vSwitches[Number].m_Status;
}

void cc_tick_deferred(SCharacterCore *pCore) {}

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
  int CurrentIndex =
      get_map_index(pCore->m_Base.m_pCollision, pCore->m_Base.m_Pos);
  pCore->m_TuneZone = is_tune(pCore->m_Base.m_pCollision, CurrentIndex);
  if (TuneZoneOld != pCore->m_TuneZone)
    pCore->m_Tuning =
        pCore->m_Base.m_pWorld
            ->m_pTuningList[pCore->m_TuneZone]; // throw tunings from specific
}

void cc_pretick(SCharacterCore *pCore) {
  cc_ddracetick(pCore);

  pCore->m_MoveRestrictions =
      get_move_restrictions(pCore->m_Base.m_pCollision, is_switch_active_cb,
                            pCore, pCore->m_Base.m_Pos);

  // get ground state
  const bool Grounded =
      check_point(pCore->m_Base.m_pCollision,
                  pCore->m_Base.m_Pos.x + PHYSICALSIZE / 2,
                  pCore->m_Base.m_Pos.y + PHYSICALSIZE / 2 + 5) ||
      check_point(pCore->m_Base.m_pCollision,
                  pCore->m_Base.m_Pos.x - PHYSICALSIZE / 2,
                  pCore->m_Base.m_Pos.y + PHYSICALSIZE / 2 + 5);

  vec2 TargetDirection =
      vnormalize((vec2){pCore->m_Input.m_TargetX, pCore->m_Input.m_TargetY});

  pCore->m_Vel.y += get_tune(pCore->m_Tuning.m_Gravity);

  float MaxSpeed = Grounded ? get_tune(pCore->m_Tuning.m_GroundControlSpeed)
                            : get_tune(pCore->m_Tuning.m_AirControlSpeed);
  float Accel = Grounded ? get_tune(pCore->m_Tuning.m_GroundControlAccel)
                         : get_tune(pCore->m_Tuning.m_AirControlAccel);
  float Friction = Grounded ? get_tune(pCore->m_Tuning.m_GroundFriction)
                            : get_tune(pCore->m_Tuning.m_AirFriction);

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
        pCore->m_Vel.y = -get_tune(pCore->m_Tuning.m_GroundJumpImpulse);
        if (pCore->m_Jumps > 1) {
          pCore->m_Jumped |= 1;
        } else {
          pCore->m_Jumped |= 3;
        }
        pCore->m_JumpedTotal = 0;
      } else if (!(pCore->m_Jumped & 2)) {
        pCore->m_Vel.y = -get_tune(pCore->m_Tuning.m_AirJumpImpulse);
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
      pCore->m_HookPos = vvadd(pCore->m_Base.m_Pos,
                               vfmul(TargetDirection, PHYSICALSIZE * 1.5f));
      pCore->m_HookDir = TargetDirection;
      pCore->m_HookedPlayer = -1;
      pCore->m_HookTick = (float)SERVER_TICK_SPEED *
                          (1.25f - get_tune(pCore->m_Tuning.m_HookDuration));
    }
  } else {
    pCore->m_HookedPlayer = -1;
    pCore->m_HookState = HOOK_IDLE;
    pCore->m_HookPos = pCore->m_Base.m_Pos;
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
    pCore->m_HookPos = pCore->m_Base.m_Pos;
  } else if (pCore->m_HookState >= HOOK_RETRACT_START &&
             pCore->m_HookState < HOOK_RETRACT_END) {
    pCore->m_HookState++;
  } else if (pCore->m_HookState == HOOK_RETRACT_END) {
    pCore->m_HookState = HOOK_RETRACTED;
  } else if (pCore->m_HookState == HOOK_FLYING) {
    vec2 HookBase = pCore->m_Base.m_Pos;
    if (pCore->m_NewHook) {
      HookBase = pCore->m_HookTeleBase;
    }
    vec2 NewPos = vvadd(
        pCore->m_HookPos,
        vfmul(pCore->m_HookDir, get_tune(pCore->m_Tuning.m_HookFireSpeed)));
    if (vdistance(HookBase, NewPos) > get_tune(pCore->m_Tuning.m_HookLength)) {
      pCore->m_HookState = HOOK_RETRACT_START;
      NewPos = vvadd(HookBase, vfmul(vnormalize(vvsub(NewPos, HookBase)),
                                     get_tune(pCore->m_Tuning.m_HookLength)));
      pCore->m_Reset = true;
    }

    // make sure that the hook doesn't go though the ground
    bool GoingToHitGround = false;
    bool GoingToRetract = false;
    bool GoingThroughTele = false;
    int teleNr = 0;
    int Hit =
        intersect_line_tele_hook(pCore->m_Base.m_pCollision, pCore->m_HookPos,
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
    if (!pCore->m_HookHitDisabled && pCore->m_Base.m_pWorld &&
        get_tune(pCore->m_Tuning.m_PlayerHooking) &&
        (pCore->m_HookState == HOOK_FLYING || !pCore->m_NewHook)) {
      float Distance = 0.0f;

      SCharacterCore *pEntity =
          (SCharacterCore *)
              pCore->m_Base.m_pWorld->m_apFirstEntityTypes[ENTTYPE_CHARACTER];
      while (pEntity) {
        if (pEntity == pCore || pEntity->m_Solo || pCore->m_Solo)
          continue;

        vec2 ClosestPoint;
        if (closest_point_on_line(pCore->m_HookPos, NewPos,
                                  pEntity->m_Base.m_Pos, &ClosestPoint)) {
          if (vdistance(pEntity->m_Base.m_Pos, ClosestPoint) <
              PHYSICALSIZE + 2.0f) {
            if (pCore->m_HookedPlayer == -1 ||
                vdistance(pCore->m_HookPos, pEntity->m_Base.m_Pos) < Distance) {
              pCore->m_HookState = HOOK_GRABBED;
              pCore->m_HookedPlayer = -1;
              Distance = vdistance(pCore->m_HookPos, pEntity->m_Base.m_Pos);
            }
          }
        }
        pEntity = (SCharacterCore *)pEntity->m_Base.m_pNextTypeEntity;
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
      vec2 *pTeleOuts =
          tele_outs(pCore->m_Base.m_pCollision, teleNr - 1, &NumOuts);
      if (GoingThroughTele && NumOuts > 0) {
        pCore->m_HookedPlayer = -1;

        pCore->m_NewHook = true;
        pCore->m_HookPos = vvadd(pTeleOuts[pCore->m_Base.m_pWorld->m_GameTick],
                                 vfmul(TargetDirection, PHYSICALSIZE * 1.5f));
        pCore->m_HookDir = TargetDirection;
        pCore->m_HookTeleBase = pCore->m_HookPos;
      } else {
        pCore->m_HookPos = NewPos;
      }
    }
  }

  if (pCore->m_HookState == HOOK_GRABBED) {
    if (pCore->m_HookedPlayer != -1 && pCore->m_Base.m_pWorld) {
      CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
      if (pCharCore && pCore->m_Id != -1)
        pCore->m_HookPos = pCharCore->m_Pos;
      else {
        // release hook
        pCore->m_HookedPlayer = -1;
        pCore->m_HookState = HOOK_RETRACTED;
        pCore->m_HookPos = m_Pos;
      }
    }

    // don't do this hook routine when we are already hooked to a player
    if (pCore->m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f) {
      vec2 HookVel = vfmul(vnormalize(m_HookPos - m_Pos),
                           get_tune(m_Tuning.m_HookDragAccel));
      // the hook as more power to drag you up then down.
      // this makes it easier to get on top of an platform
      if (HookVel.y > 0)
        HookVel.y *= 0.3f;

      // the hook will boost it's power if the player wants to move
      // in that direction. otherwise it will dampen everything abit
      if ((HookVel.x < 0 && m_Direction < 0) ||
          (HookVel.x > 0 && m_Direction > 0))
        HookVel.x *= 0.95f;
      else
        HookVel.x *= 0.75f;

      vec2 NewVel = vvadd(m_Vel, HookVel);

      // check if we are under the legal limit for the hook
      const float NewVelLength = vlength(NewVel);
      if (NewVelLength < get_tune(m_Tuning.m_HookDragSpeed) ||
          NewVelLength < vlength(m_Vel))
        pCore->m_Vel = NewVel; // no problem. apply
    }

    // release hook (max default hook time is 1.25 s)
    pCore->m_HookTick++;
    if (pCore->m_HookedPlayer != -1 &&
        (pCore->m_HookTick > SERVER_TICK_SPEED + SERVER_TICK_SPEED / 5 ||
         (m_pWorld && !m_pWorld->m_apCharacters[m_HookedPlayer]))) {
      pCore->m_HookedPlayer = -1;
      pCore->m_HookState = HOOK_RETRACTED;
      pCore->m_HookPos = m_Pos;
    }
  }

  if (!pCore->m_Base.m_pWorld->m_NoWeakHookAndBounce)
    TickDeferred();
}

void cc_tick(SCharacterCore *pCore) {}
void cc_move(SCharacterCore *pCore) {}

void cc_quantize(SCharacterCore *pCore) {
  pCore->m_Base.m_Pos.x = round_to_int(pCore->m_Base.m_Pos.x);
  pCore->m_Base.m_Pos.y = round_to_int(pCore->m_Base.m_Pos.y);
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

  // TODO: figure out highest switch number in collision later
  init_switchers(pCore, 0);
  init_tunings(&pCore->m_Tuning);
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
    case ENTTYPE_FLAG: {
    }
    case ENTTYPE_CHARACTER: {
    }
      if (pCore->m_NoWeakHook) {
        SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
        while (pEntity) {
          cc_pretick((SCharacterCore *)pEntity);
          pEntity = pEntity->m_pNextTypeEntity;
        }
      }
      SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
      while (pEntity) {
        cc_tick((SCharacterCore *)pEntity);
        pEntity = pEntity->m_pNextTypeEntity;
      }
    }
  }

  // Do tick deferred
  for (int i = 0; i < NUM_ENTTYPES; ++i) {
    switch (i) {
    case ENTTYPE_PROJECTILE: {
    }
    case ENTTYPE_LASER: {
    }
    case ENTTYPE_PICKUP: {
    }
    case ENTTYPE_FLAG: {
    }
    case ENTTYPE_CHARACTER: {
    }
      SEntity *pEntity = pCore->m_apFirstEntityTypes[i];
      while (pEntity) {
        cc_tick_deferred((SCharacterCore *)pEntity);
        pEntity = pEntity->m_pNextTypeEntity;
      }
    }
  }
}

// }}}
