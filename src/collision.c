#include "collision.h"
#include "vmath.h"
#include <stdio.h>

enum {
  MR_DIR_HERE = 0,
  MR_DIR_RIGHT,
  MR_DIR_DOWN,
  MR_DIR_LEFT,
  MR_DIR_UP,
  NUM_MR_DIRS
};

int get_pure_map_index(SCollision *pCollision, vec2 Pos) {
  const int nx = iclamp(round_to_int(Pos.x) / 32, 0, pCollision->m_Width - 1);
  const int ny = iclamp(round_to_int(Pos.y) / 32, 0, pCollision->m_Height - 1);
  return ny * pCollision->m_Width + nx;
}

int get_move_restrictions_mask(int Direction) {
  static const int aDirections[NUM_MR_DIRS] = {0, CANTMOVE_RIGHT, CANTMOVE_DOWN,
                                               CANTMOVE_LEFT, CANTMOVE_UP};
  return aDirections[Direction];
}

int get_move_restrictions_raw(int Tile, int Flags) {
  Flags &= (TILEFLAG_XFLIP | TILEFLAG_YFLIP | TILEFLAG_ROTATE);
  switch (Tile) {
  case TILE_STOP: {
    static const int move_table[] = {
        CANTMOVE_DOWN, // 0: ROTATION_0
        CANTMOVE_DOWN, // 1: TILEFLAG_YFLIP ^ ROTATION_180
        CANTMOVE_UP,   // 2: TILEFLAG_YFLIP ^ ROTATION_0
        CANTMOVE_UP,   // 3: ROTATION_180
        0,
        0,
        0,
        0,              // 4-7: unused
        CANTMOVE_LEFT,  // 8: ROTATION_90
        CANTMOVE_LEFT,  // 9: TILEFLAG_YFLIP ^ ROTATION_270
        CANTMOVE_RIGHT, // 10: TILEFLAG_YFLIP ^ ROTATION_90
        CANTMOVE_RIGHT  // 11: ROTATION_270
    };
    return move_table[Flags];
  }
  case TILE_STOPS:
    return (Flags & TILEFLAG_ROTATE) ? (CANTMOVE_LEFT | CANTMOVE_RIGHT)
                                     : (CANTMOVE_DOWN | CANTMOVE_UP);
  case TILE_STOPA:
    return CANTMOVE_LEFT | CANTMOVE_RIGHT | CANTMOVE_UP | CANTMOVE_DOWN;
  default:
    return 0;
  }
}

int move_restrictions(int Direction, int Tile, int Flags) {
  int Result = get_move_restrictions_raw(Tile, Flags);
  if (Direction == MR_DIR_HERE && Tile == TILE_STOP)
    return Result;
  return Result & get_move_restrictions_mask(Direction);
}

int get_tile_index(SCollision *pCollision, int Index) {
  return pCollision->m_GameLayer.m_pData[Index];
}
int get_front_tile_index(SCollision *pCollision, int Index) {
  return pCollision->m_FrontLayer.m_pData[Index];
}
int get_tile_flags(SCollision *pCollision, int Index) {
  return pCollision->m_GameLayer.m_pFlags[Index];
}
int get_front_tile_flags(SCollision *pCollision, int Index) {
  return pCollision->m_FrontLayer.m_pFlags[Index];
}
int get_switch_number(SCollision *pCollision, int Index) {
  return pCollision->m_SwitchLayer.m_pNumber[Index];
}
int get_switch_type(SCollision *pCollision, int Index) {
  return pCollision->m_SwitchLayer.m_pType[Index];
}
int get_switch_delay(SCollision *pCollision, int Index) {
  return pCollision->m_SwitchLayer.m_pDelay[Index];
}
int is_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELEIN
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
int is_teleport_hook(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELEINHOOK
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
int is_teleport_weapon(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELEINWEAPON
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
int is_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELEINEVIL
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
bool is_check_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELECHECKIN
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
bool is_check_evil_teleport(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELECHECKINEVIL
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
int is_tele_checkpoint(SCollision *pCollision, int Index) {
  return pCollision->m_TeleLayer.m_pType[Index] == TILE_TELECHECK
             ? pCollision->m_TeleLayer.m_pNumber[Index]
             : 0;
}
int get_collision_at(SCollision *pCollision, float x, float y) {
  int Nx = iclamp(x / 32, 0, pCollision->m_Width - 1);
  int Ny = iclamp(y / 32, 0, pCollision->m_Height - 1);
  int pos = Ny * pCollision->m_Width + Nx;
  int Idx = pCollision->m_GameLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}
int get_front_collision_at(SCollision *pCollision, float x, float y) {
  int Nx = iclamp(x / 32, 0, pCollision->m_Width - 1);
  int Ny = iclamp(y / 32, 0, pCollision->m_Height - 1);
  int pos = Ny * pCollision->m_Width + Nx;
  int Idx = pCollision->m_FrontLayer.m_pData[pos];
  if (Idx >= TILE_SOLID && Idx <= TILE_NOLASER)
    return Idx;
  return 0;
}
bool tile_exists_next(SCollision *pCollision, int Index) {
  const unsigned char *pTileIdx = pCollision->m_GameLayer.m_pData;
  const unsigned char *pTileFlgs = pCollision->m_GameLayer.m_pFlags;
  const unsigned char *pFrontIdx = pCollision->m_FrontLayer.m_pData;
  const unsigned char *pFrontFlgs = pCollision->m_FrontLayer.m_pFlags;
  const unsigned char *pDoorIdx = pCollision->m_DoorLayer.m_pIndex;
  const unsigned char *pDoorFlgs = pCollision->m_DoorLayer.m_pFlags;
  int TileOnTheLeft = (Index - 1 > 0) ? Index - 1 : Index;
  int TileOnTheRight = (Index + 1 < pCollision->m_Width * pCollision->m_Height)
                           ? Index + 1
                           : Index;
  int TileBelow =
      (Index + pCollision->m_Width < pCollision->m_Width * pCollision->m_Height)
          ? Index + pCollision->m_Width
          : Index;
  int TileAbove =
      (Index - pCollision->m_Width > 0) ? Index - pCollision->m_Width : Index;

  if ((pTileIdx[TileOnTheRight] == TILE_STOP &&
       pTileFlgs[TileOnTheRight] == ROTATION_270) ||
      (pTileIdx[TileOnTheLeft] == TILE_STOP &&
       pTileFlgs[TileOnTheLeft] == ROTATION_90))
    return true;
  if ((pTileIdx[TileBelow] == TILE_STOP &&
       pTileFlgs[TileBelow] == ROTATION_0) ||
      (pTileIdx[TileAbove] == TILE_STOP &&
       pTileFlgs[TileAbove] == ROTATION_180))
    return true;
  if (pTileIdx[TileOnTheRight] == TILE_STOPA ||
      pTileIdx[TileOnTheLeft] == TILE_STOPA ||
      ((pTileIdx[TileOnTheRight] == TILE_STOPS ||
        pTileIdx[TileOnTheLeft] == TILE_STOPS)))
    return true;
  if (pTileIdx[TileBelow] == TILE_STOPA || pTileIdx[TileAbove] == TILE_STOPA ||
      ((pTileIdx[TileBelow] == TILE_STOPS ||
        pTileIdx[TileAbove] == TILE_STOPS) &&
       pTileFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
    return true;

  if (pFrontIdx) {
    if (pFrontIdx[TileOnTheRight] == TILE_STOPA ||
        pFrontIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pFrontIdx[TileOnTheRight] == TILE_STOPS ||
          pFrontIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pFrontIdx[TileBelow] == TILE_STOPA ||
        pFrontIdx[TileAbove] == TILE_STOPA ||
        ((pFrontIdx[TileBelow] == TILE_STOPS ||
          pFrontIdx[TileAbove] == TILE_STOPS) &&
         pFrontFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pFrontIdx[TileOnTheRight] == TILE_STOP &&
         pFrontFlgs[TileOnTheRight] == ROTATION_270) ||
        (pFrontIdx[TileOnTheLeft] == TILE_STOP &&
         pFrontFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pFrontIdx[TileBelow] == TILE_STOP &&
         pFrontFlgs[TileBelow] == ROTATION_0) ||
        (pFrontIdx[TileAbove] == TILE_STOP &&
         pFrontFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  if (pDoorIdx) {
    if (pDoorIdx[TileOnTheRight] == TILE_STOPA ||
        pDoorIdx[TileOnTheLeft] == TILE_STOPA ||
        ((pDoorIdx[TileOnTheRight] == TILE_STOPS ||
          pDoorIdx[TileOnTheLeft] == TILE_STOPS)))
      return true;
    if (pDoorIdx[TileBelow] == TILE_STOPA ||
        pDoorIdx[TileAbove] == TILE_STOPA ||
        ((pDoorIdx[TileBelow] == TILE_STOPS ||
          pDoorIdx[TileAbove] == TILE_STOPS) &&
         pDoorFlgs[TileBelow] | ROTATION_180 | ROTATION_0))
      return true;
    if ((pDoorIdx[TileOnTheRight] == TILE_STOP &&
         pDoorFlgs[TileOnTheRight] == ROTATION_270) ||
        (pDoorIdx[TileOnTheLeft] == TILE_STOP &&
         pDoorFlgs[TileOnTheLeft] == ROTATION_90))
      return true;
    if ((pDoorIdx[TileBelow] == TILE_STOP &&
         pDoorFlgs[TileBelow] == ROTATION_0) ||
        (pDoorIdx[TileAbove] == TILE_STOP &&
         pDoorFlgs[TileAbove] == ROTATION_180))
      return true;
  }
  return false;
}

bool tile_exists(SCollision *pCollision, int Index) {
  const unsigned char *pTiles = pCollision->m_GameLayer.m_pData;
  const unsigned char *pFront = pCollision->m_FrontLayer.m_pData;
  const unsigned char *pDoor = pCollision->m_DoorLayer.m_pIndex;
  const unsigned char *pTele = pCollision->m_TeleLayer.m_pType;
  const unsigned char *pSpeedup = pCollision->m_SpeedupLayer.m_pForce;
  const unsigned char *pSwitch = pCollision->m_SwitchLayer.m_pType;
  const unsigned char *pTune = pCollision->m_TuneLayer.m_pType;

  if ((pTiles[Index] >= TILE_FREEZE &&
       pTiles[Index] <= TILE_TELE_LASER_DISABLE) ||
      (pTiles[Index] >= TILE_LFREEZE && pTiles[Index] <= TILE_LUNFREEZE))
    return true;
  if (pFront &&
      ((pFront[Index] >= TILE_FREEZE &&
        pFront[Index] <= TILE_TELE_LASER_DISABLE) ||
       (pFront[Index] >= TILE_LFREEZE && pFront[Index] <= TILE_LUNFREEZE)))
    return true;
  if (pTele &&
      (pTele[Index] == TILE_TELEIN || pTele[Index] == TILE_TELEINEVIL ||
       pTele[Index] == TILE_TELECHECKINEVIL || pTele[Index] == TILE_TELECHECK ||
       pTele[Index] == TILE_TELECHECKIN))
    return true;
  if (pSpeedup && pSpeedup[Index] > 0)
    return true;
  if (pDoor && pDoor[Index])
    return true;
  if (pSwitch && pSwitch[Index])
    return true;
  if (pTune && pTune[Index])
    return true;
  return tile_exists_next(pCollision, Index);
}

int get_move_restrictions(SCollision *pCollision,
                          CALLBACK_SWITCHACTIVE pfnSwitchActive, void *pUser,
                          vec2 Pos, float Distance,
                          int OverrideCenterTileIndex) {
  const vec2 DIRECTIONS[NUM_MR_DIRS] = {vec2_init(0, 0), vec2_init(1, 0),
                                        vec2_init(0, 1), vec2_init(-1, 0),
                                        vec2_init(0, -1)};
  int Restrictions = 0;
  for (int d = 0; d < NUM_MR_DIRS; d++) {
    vec2 ModPos = vvadd(Pos, vfmul(DIRECTIONS[d], Distance));
    int ModMapIndex = get_pure_map_index(pCollision, ModPos);
    if (d == MR_DIR_HERE && OverrideCenterTileIndex >= 0) {
      ModMapIndex = OverrideCenterTileIndex;
    }
    for (int Front = 0; Front < 2 - !(pCollision->m_FrontLayer.m_pData);
         Front++) {
      int Tile;
      int Flags;
      if (!Front) {
        Tile = get_tile_index(pCollision, ModMapIndex);
        Flags = get_tile_flags(pCollision, ModMapIndex);
      } else {
        Tile = get_front_tile_index(pCollision, ModMapIndex);
        Flags = get_front_tile_flags(pCollision, ModMapIndex);
      }
      Restrictions |= move_restrictions(d, Tile, Flags);
    }
    if (pfnSwitchActive) {
      if (pCollision->m_DoorLayer.m_pIndex && ModMapIndex >= 0 &&
          pCollision->m_DoorLayer.m_pIndex[ModMapIndex]) {
        if (pfnSwitchActive(pCollision->m_DoorLayer.m_pNumber[ModMapIndex],
                            pUser)) {
          Restrictions |= move_restrictions(
              d, pCollision->m_DoorLayer.m_pIndex[ModMapIndex],
              pCollision->m_DoorLayer.m_pFlags[ModMapIndex]);
        }
      }
    }
  }
  return Restrictions;
}

int get_map_index(SCollision *pCollision, vec2 Pos) {
  int Nx = iclamp((int)Pos.x / 32, 0, pCollision->m_Width - 1);
  int Ny = iclamp((int)Pos.y / 32, 0, pCollision->m_Height - 1);
  int Index = Ny * pCollision->m_Width + Nx;

  if (tile_exists(pCollision, Index))
    return Index;
  else
    return -1;
}

bool check_point(SCollision *pCollision, vec2 Pos) {
  int Nx = iclamp(round_to_int(Pos.x) / 32, 0, pCollision->m_Width - 1);
  int Ny = iclamp(round_to_int(Pos.y) / 32, 0, pCollision->m_Height - 1);
  int Idx = pCollision->m_GameLayer.m_pData[Ny * pCollision->m_Width + Nx];
  return Idx == TILE_SOLID || Idx == TILE_NOHOOK;
}

void ThroughOffset(vec2 Pos0, vec2 Pos1, int *pOffsetX, int *pOffsetY) {
  float x = Pos0.x - Pos1.x;
  float y = Pos0.y - Pos1.y;
  if (fabs(x) > fabs(y)) {
    if (x < 0) {
      *pOffsetX = -32;
      *pOffsetY = 0;
    } else {
      *pOffsetX = 32;
      *pOffsetY = 0;
    }
  } else {
    if (y < 0) {
      *pOffsetX = 0;
      *pOffsetY = -32;
    } else {
      *pOffsetX = 0;
      *pOffsetY = 32;
    }
  }
}

bool is_through(SCollision *pCollision, int x, int y, int OffsetX, int OffsetY,
                vec2 Pos0, vec2 Pos1) {
  int pos = get_pure_map_index(pCollision, vec2_init(x, y));

  unsigned char *pFrontIdx = pCollision->m_FrontLayer.m_pData;
  unsigned char *pFrontFlgs = pCollision->m_FrontLayer.m_pFlags;
  if (pFrontIdx && (pFrontIdx[pos] == TILE_THROUGH_ALL ||
                    pFrontIdx[pos] == TILE_THROUGH_CUT))
    return true;
  if (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[pos] == ROTATION_0 && Pos0.y > Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_90 && Pos0.x < Pos1.x) ||
       (pFrontFlgs[pos] == ROTATION_180 && Pos0.y < Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_270 && Pos0.x > Pos1.x)))
    return true;
  int offpos =
      get_pure_map_index(pCollision, vec2_init(x + OffsetX, y + OffsetY));
  unsigned char *pTileIdx = pCollision->m_GameLayer.m_pData;
  return pTileIdx[offpos] == TILE_THROUGH ||
         (pFrontIdx && pFrontIdx[offpos] == TILE_THROUGH);
}

bool is_hook_blocker(SCollision *pCollision, int x, int y, vec2 Pos0,
                     vec2 Pos1) {
  int pos = get_pure_map_index(pCollision, vec2_init(x, y));
  unsigned char *pTileIdx = pCollision->m_GameLayer.m_pData;
  unsigned char *pTileFlgs = pCollision->m_GameLayer.m_pData;

  unsigned char *pFrontIdx = pCollision->m_FrontLayer.m_pData;
  unsigned char *pFrontFlgs = pCollision->m_FrontLayer.m_pFlags;
  if (pTileIdx[pos] == TILE_THROUGH_ALL ||
      (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_ALL))
    return true;
  if (pTileIdx[pos] == TILE_THROUGH_DIR &&
      ((pTileFlgs[pos] == ROTATION_0 && Pos0.y < Pos1.y) ||
       (pTileFlgs[pos] == ROTATION_90 && Pos0.x > Pos1.x) ||
       (pTileFlgs[pos] == ROTATION_180 && Pos0.y > Pos1.y) ||
       (pTileFlgs[pos] == ROTATION_270 && Pos0.x < Pos1.x)))
    return true;
  if (pFrontIdx && pFrontIdx[pos] == TILE_THROUGH_DIR &&
      ((pFrontFlgs[pos] == ROTATION_0 && Pos0.y < Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_90 && Pos0.x > Pos1.x) ||
       (pFrontFlgs[pos] == ROTATION_180 && Pos0.y > Pos1.y) ||
       (pFrontFlgs[pos] == ROTATION_270 && Pos0.x < Pos1.x)))
    return true;
  return false;
}

int intersect_line_tele_hook_noclamp(SCollision *pCollision, vec2 Pos0,
                                     vec2 Pos1, vec2 *pOutCollision,
                                     int *pTeleNr, bool OldTeleHook) {
  float Distance = vdistance(Pos0, Pos1);
  int End = Distance + 1;
  int dx = 0, dy = 0; // Offset for checking the "through" tile
  ThroughOffset(Pos0, Pos1, &dx, &dy);
  int LastIndex = 0;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    // Temporary position for checking collision
    int ix = round_to_int(Pos.x);
    int iy = round_to_int(Pos.y);
    const int nx = iclamp(ix / 32, 0, pCollision->m_Width - 1);
    const int ny = iclamp(iy / 32, 0, pCollision->m_Height - 1);
    int Index = ny * pCollision->m_Width + nx;
    if (Index == LastIndex)
      continue;
    LastIndex = Index;
    if (pTeleNr) {
      if (OldTeleHook)
        *pTeleNr = is_teleport(pCollision, Index);
      else
        *pTeleNr = is_teleport_hook(pCollision, Index);
      if (*pTeleNr) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return TILE_TELEINHOOK;
      }
    }

    if (check_point(pCollision, vec2_init(ix, iy))) {
      if (!is_through(pCollision, ix, iy, dx, dy, Pos0, Pos1)) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return get_collision_at(pCollision, ix, iy);
      }
    } else if (is_hook_blocker(pCollision, ix, iy, Pos0, Pos1)) {
      if (pOutCollision)
        *pOutCollision = Pos;
      return TILE_NOHOOK;
    }
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  return 0;
}
int intersect_line_tele_hook(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                             vec2 *pOutCollision, int *pTeleNr,
                             bool OldTeleHook) {
  float Distance = vdistance(Pos0, Pos1);
  int End = Distance + 1;
  int dx = 0, dy = 0; // Offset for checking the "through" tile
  ThroughOffset(Pos0, Pos1, &dx, &dy);
  int LastIndex = 0;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    // Temporary position for checking collision
    int ix = round_to_int(Pos.x);
    int iy = round_to_int(Pos.y);
    const int nx = iclamp(ix >> 5, 0, pCollision->m_Width - 1);
    const int ny = iclamp(iy >> 5, 0, pCollision->m_Height - 1);
    int Index = ny * pCollision->m_Width + nx;

    // behind this is basically useless to optimize
    if (Index == LastIndex)
      continue;
    LastIndex = Index;
    if (pTeleNr) {
      if (OldTeleHook)
        *pTeleNr = is_teleport(pCollision, Index);
      else
        *pTeleNr = is_teleport_hook(pCollision, Index);
      if (*pTeleNr) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return TILE_TELEINHOOK;
      }
    }

    if (check_point(pCollision, vec2_init(ix, iy))) {
      if (!is_through(pCollision, ix, iy, dx, dy, Pos0, Pos1)) {
        if (pOutCollision)
          *pOutCollision = Pos;
        return get_collision_at(pCollision, ix, iy);
      }
    } else if (is_hook_blocker(pCollision, ix, iy, Pos0, Pos1)) {
      if (pOutCollision)
        *pOutCollision = Pos;
      return TILE_NOHOOK;
    }
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  return 0;
}

bool test_box(SCollision *pCollision, vec2 Pos, vec2 Size) {
  Size = vfmul(Size, 0.5f);
  if (check_point(pCollision, vec2_init(Pos.x - Size.x, Pos.y - Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x + Size.x, Pos.y - Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x - Size.x, Pos.y + Size.y)))
    return true;
  if (check_point(pCollision, vec2_init(Pos.x + Size.x, Pos.y + Size.y)))
    return true;
  return false;
}

int is_tune(SCollision *pCollision, int Index) {
  if (!pCollision->m_TuneLayer.m_pType)
    return 0;
  if (pCollision->m_TuneLayer.m_pType[Index])
    return pCollision->m_TuneLayer.m_pNumber[Index];
  return 0;
}

bool is_speedup(SCollision *pCollision, int Index) {
  return pCollision->m_SpeedupLayer.m_pType &&
         pCollision->m_SpeedupLayer.m_pForce[Index] > 0;
}

void get_speedup(SCollision *pCollision, int Index, vec2 *pDir, int *pForce,
                 int *pMaxSpeed, int *pType) {
  float Angle = pCollision->m_SpeedupLayer.m_pAngle[Index] * (PI / 180.0f);
  *pForce = pCollision->m_SpeedupLayer.m_pForce[Index];
  *pType = pCollision->m_SpeedupLayer.m_pType[Index];
  *pDir = vdirection(Angle);
  if (pMaxSpeed)
    *pMaxSpeed = pCollision->m_SpeedupLayer.m_pMaxSpeed[Index];
}

const vec2 *spawn_points(SCollision *pCollision, int *pOutNum) {
  *pOutNum = pCollision->m_NumSpawnPoints;
  return (vec2 *)pCollision->m_pSpawnPoints;
}

const vec2 *tele_outs(SCollision *pCollision, int Number, int *pOutNum) {
  *pOutNum = pCollision->m_aNumTeleOuts[Number];
  return (vec2 *)pCollision->m_apTeleOuts[Number];
}
const vec2 *tele_check_outs(SCollision *pCollision, int Number, int *pOutNum) {
  *pOutNum = pCollision->m_aNumTeleCheckOuts[Number];
  return (vec2 *)pCollision->m_apTeleCheckOuts[Number];
}

int intersect_line(SCollision *pCollision, vec2 Pos0, vec2 Pos1,
                   vec2 *pOutCollision, vec2 *pOutBeforeCollision) {
  float Distance = vdistance(Pos0, Pos1);
  int End = Distance + 1;
  vec2 Last = Pos0;
  for (int i = 0; i <= End; i++) {
    float a = i / (float)End;
    vec2 Pos = vvfmix(Pos0, Pos1, a);
    // Temporary position for checking collision
    int ix = round_to_int(Pos.x);
    int iy = round_to_int(Pos.y);

    if (check_point(pCollision, vec2_init(ix, iy))) {
      if (pOutCollision)
        *pOutCollision = Pos;
      if (pOutBeforeCollision)
        *pOutBeforeCollision = Last;
      return get_collision_at(pCollision, ix, iy);
    }

    Last = Pos;
  }
  if (pOutCollision)
    *pOutCollision = Pos1;
  if (pOutBeforeCollision)
    *pOutBeforeCollision = Pos1;
  return 0;
}

void move_box(SCollision *pCollision, vec2 *pInoutPos, vec2 *pInoutVel,
              vec2 Size, vec2 Elasticity, bool *pGrounded) {
  vec2 Pos = *pInoutPos;
  vec2 Vel = *pInoutVel;
  float Distance = vlength(Vel);
  if (Distance <= 0.00001f)
    return;

  int Max = (int)Distance;
  float Fraction = 1.0f / (float)(Max + 1);
  float ElasticityX = fclamp(Elasticity.x, -1.0f, 1.0f);
  float ElasticityY = fclamp(Elasticity.y, -1.0f, 1.0f);

  for (int i = 0; i <= Max; i++) {
    vec2 NewPos = vvadd(Pos, vfmul(Vel, Fraction));
    if (test_box(pCollision, NewPos, Size)) {
      int Hits = 0;
      if (test_box(pCollision, vec2_init(Pos.x, NewPos.y), Size)) {
        if (pGrounded && ElasticityY > 0 && Vel.y > 0)
          *pGrounded = true;
        NewPos.y = Pos.y;
        Vel.y *= -ElasticityY;
        Hits++;
      }
      if (test_box(pCollision, vec2_init(NewPos.x, Pos.y), Size)) {
        NewPos.x = Pos.x;
        Vel.x *= -ElasticityX;
        Hits++;
      }
      if (Hits == 0) { // Corner case
        if (pGrounded && ElasticityY > 0 && Vel.y > 0)
          *pGrounded = true;
        NewPos = Pos;
        Vel.x *= -ElasticityX;
        Vel.y *= -ElasticityY;
      }
    }
    Pos = NewPos;
    if (vlength(Vel) < 0.00001f)
      break;
  }

  *pInoutPos = Pos;
  *pInoutVel = Vel;
}

bool get_nearest_air_pos_player(SCollision *pCollision, vec2 PlayerPos,
                                vec2 *pOutPos) {
  for (int dist = 5; dist >= -1; dist--) {
    *pOutPos = vec2_init(PlayerPos.x, PlayerPos.y - dist);
    if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC)) {
      return true;
    }
  }
  return false;
}

bool get_nearest_air_pos(SCollision *pCollision, vec2 Pos, vec2 PrevPos,
                         vec2 *pOutPos) {
  for (int k = 0; k < 16 && check_point(pCollision, Pos); k++) {
    Pos = vvsub(Pos, vnormalize(vvsub(PrevPos, Pos)));
  }

  vec2 PosInBlock =
      vec2_init(round_to_int(Pos.x) % 32, round_to_int(Pos.y) % 32);
  vec2 BlockCenter = vfadd(
      vvsub(vec2_init(round_to_int(Pos.x), round_to_int(Pos.y)), PosInBlock),
      16.0f);

  *pOutPos =
      vec2_init(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f), Pos.y);
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos =
      vec2_init(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
  if (!test_box(pCollision, *pOutPos, PHYSICALSIZEVEC))
    return true;

  *pOutPos = vec2_init(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
                       BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
  return !test_box(pCollision, *pOutPos, PHYSICALSIZEVEC);
}

int get_index(SCollision *pCollision, vec2 PrevPos, vec2 Pos) {
  float Distance = vdistance(PrevPos, Pos);

  if (!Distance) {
    int Nx = iclamp((int)Pos.x / 32, 0, pCollision->m_Width - 1);
    int Ny = iclamp((int)Pos.y / 32, 0, pCollision->m_Height - 1);

    if (pCollision->m_TeleLayer.m_pType ||
        (pCollision->m_SpeedupLayer.m_pForce &&
         pCollision->m_SpeedupLayer.m_pForce[Ny * pCollision->m_Width + Nx] >
             0)) {
      return Ny * pCollision->m_Width + Nx;
    }
  }

  for (int i = 0, id = ceil(Distance); i < id; i++) {
    float a = (float)i / Distance;
    vec2 Tmp = vvfmix(PrevPos, Pos, a);
    int Nx = iclamp((int)Tmp.x / 32, 0, pCollision->m_Width - 1);
    int Ny = iclamp((int)Tmp.y / 32, 0, pCollision->m_Height - 1);
    if (pCollision->m_TeleLayer.m_pType ||
        (pCollision->m_SpeedupLayer.m_pForce &&
         pCollision->m_SpeedupLayer.m_pForce[Ny * pCollision->m_Width + Nx] >
             0)) {
      return Ny * pCollision->m_Width + Nx;
    }
  }

  return -1;
}

int mover_speed(SCollision *pCollision, int x, int y, vec2 *pSpeed) {
  int Nx = iclamp(x / 32, 0, pCollision->m_Width - 1);
  int Ny = iclamp(y / 32, 0, pCollision->m_Height - 1);
  int Index = pCollision->m_GameLayer.m_pData[Ny * pCollision->m_Width + Nx];

  if (Index != TILE_CP && Index != TILE_CP_F) {
    return 0;
  }

  vec2 Target;
  switch (pCollision->m_GameLayer.m_pFlags[Ny * pCollision->m_Width + Nx]) {
  case ROTATION_0:
    Target.x = 0.0f;
    Target.y = -4.0f;
    break;
  case ROTATION_90:
    Target.x = 4.0f;
    Target.y = 0.0f;
    break;
  case ROTATION_180:
    Target.x = 0.0f;
    Target.y = 4.0f;
    break;
  case ROTATION_270:
    Target.x = -4.0f;
    Target.y = 0.0f;
    break;
  default:
    Target = vec2_init(0.0f, 0.0f);
    break;
  }
  if (Index == TILE_CP_F) {
    Target = vfmul(Target, 4.0f);
  }
  *pSpeed = Target;
  return Index;
}

int entity(SCollision *pCollision, int x, int y, int Layer) {
  if (x < 0 || x >= pCollision->m_Width || y < 0 || y >= pCollision->m_Height)
    return 0;

  const int Index = y * pCollision->m_Width + x;
  switch (Layer) {
  case LAYER_GAME:
    return pCollision->m_GameLayer.m_pData[Index] - ENTITY_OFFSET;
  case LAYER_FRONT:
    return pCollision->m_FrontLayer.m_pData[Index] - ENTITY_OFFSET;
  case LAYER_SWITCH:
    return pCollision->m_SwitchLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_TELE:
    return pCollision->m_TeleLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_SPEEDUP:
    return pCollision->m_SpeedupLayer.m_pType[Index] - ENTITY_OFFSET;
  case LAYER_TUNE:
    return pCollision->m_TuneLayer.m_pType[Index] - ENTITY_OFFSET;
  default:
    printf("Error while initializing gameworld: invalid layer found\n");
  }
  return 0;
}
